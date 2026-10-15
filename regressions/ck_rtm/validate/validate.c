/*
 * Copyright 2026 Samy Al Bahra.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Validates the ck_elide wrappers for both the RTM and non-RTM
 * (CK_F_PR_RTM undefined) implementations. All assertions below hold
 * under either implementation:
 *   - Trylock on a held lock must fail (under RTM, the lock predicate
 *     forces an explicit abort; without RTM, the underlying trylock
 *     fails) and must not disturb lock state.
 *   - Trylock on an uncontended lock must be able to succeed, and
 *     unlock must restore the lock to an unowned state. Under RTM a
 *     transaction may abort spuriously, so success is asserted over a
 *     bounded number of attempts rather than per-attempt.
 *   - Lock operations must provide mutual exclusion. Under RTM,
 *     elided sections are atomic so invariant violations cannot be
 *     observed from within a section; the post-join counter check
 *     below holds either way.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <ck_elide.h>
#include <ck_pr.h>
#include <ck_spinlock.h>

#include "../../common.h"

#ifndef ITERATE
#define ITERATE 1000000
#endif

#define TRYLOCK_ATTEMPTS 4096

static struct affinity a;
static unsigned int locked = 0;
static unsigned int entries = 0;
static int nthr;
static ck_spinlock_t lock = CK_SPINLOCK_INITIALIZER;

static void
critical_section(void)
{
	unsigned int l;

	l = ck_pr_load_uint(&locked);
	if (l != 0) {
		ck_error("ERROR [%d]: %u != 0\n", __LINE__, l);
	}

	ck_pr_inc_uint(&locked);
	ck_pr_inc_uint(&locked);
	ck_pr_inc_uint(&locked);
	ck_pr_inc_uint(&locked);

	l = ck_pr_load_uint(&locked);
	if (l != 4) {
		ck_error("ERROR [%d]: %u != 4\n", __LINE__, l);
	}

	ck_pr_dec_uint(&locked);
	ck_pr_dec_uint(&locked);
	ck_pr_dec_uint(&locked);
	ck_pr_dec_uint(&locked);

	l = ck_pr_load_uint(&locked);
	if (l != 0) {
		ck_error("ERROR [%d]: %u != 0\n", __LINE__, l);
	}

	return;
}

static void *
thread(void *null CK_CC_UNUSED)
{
	unsigned int n_entries = 0;
	int i = ITERATE;

	if (aff_iterate(&a)) {
		perror("ERROR: Could not affine thread");
		exit(EXIT_FAILURE);
	}

	while (i--) {
		/*
		 * Under RTM, an abort after a successful trylock rolls
		 * the section back and resumes here with a false return
		 * value, in which case we acquire through the lock slow
		 * path. Either way, the section below executes exactly
		 * once per iteration.
		 */
		if (CK_ELIDE_TRYLOCK(ck_spinlock, &lock) == false)
			CK_ELIDE_LOCK(ck_spinlock, &lock);

		critical_section();
		n_entries++;

		CK_ELIDE_UNLOCK(ck_spinlock, &lock);
	}

	ck_pr_add_uint(&entries, n_entries);
	return (NULL);
}

int
main(int argc, char *argv[])
{
	pthread_t *threads;
	unsigned int i, n_success;

	/*
	 * Trylock on a held lock must fail without disturbing lock
	 * state. This is exercised through the underlying lock so that
	 * the result is deterministic with and without RTM.
	 */
	ck_spinlock_lock(&lock);
	for (i = 0; i < TRYLOCK_ATTEMPTS; i++) {
		if (CK_ELIDE_TRYLOCK(ck_spinlock, &lock) == true) {
			ck_error("ERROR: Trylock succeeded on a held lock\n");
		}
	}

	if (ck_spinlock_locked(&lock) == false) {
		ck_error("ERROR: Failed trylock released the lock\n");
	}
	ck_spinlock_unlock(&lock);

	/* Trylock on an uncontended lock must be able to succeed. */
	n_success = 0;
	for (i = 0; i < TRYLOCK_ATTEMPTS; i++) {
		if (CK_ELIDE_TRYLOCK(ck_spinlock, &lock) == false)
			continue;

		n_success++;
		CK_ELIDE_UNLOCK(ck_spinlock, &lock);
	}

	if (n_success == 0) {
		ck_error("ERROR: Trylock never acquired an uncontended lock\n");
	}

	if (ck_spinlock_locked(&lock) == true) {
		ck_error("ERROR: Lock still owned after unlock\n");
	}

	if (argc != 3) {
		ck_error("Usage: validate <number of threads> <affinity delta>\n");
	}

	nthr = atoi(argv[1]);
	if (nthr <= 0) {
		ck_error("ERROR: Number of threads must be greater than 0\n");
	}

	threads = malloc(sizeof(pthread_t) * nthr);
	if (threads == NULL) {
		ck_error("ERROR: Could not allocate thread structures\n");
	}

	a.delta = atoi(argv[2]);

	fprintf(stderr, "Creating threads (mutual exclusion)...");
	for (i = 0; i < (unsigned int)nthr; i++) {
		if (pthread_create(&threads[i], NULL, thread, NULL)) {
			ck_error("ERROR: Could not create thread %d\n", i);
		}
	}
	fprintf(stderr, "done\n");

	fprintf(stderr, "Waiting for threads to finish correctness regression...");
	for (i = 0; i < (unsigned int)nthr; i++)
		pthread_join(threads[i], NULL);

	if (ck_pr_load_uint(&entries) != (unsigned int)nthr * ITERATE) {
		ck_error("ERROR: %u != %u critical section entries\n",
		    ck_pr_load_uint(&entries), (unsigned int)nthr * ITERATE);
	}

	if (ck_pr_load_uint(&locked) != 0) {
		ck_error("ERROR: Lock state %u != 0 after join\n",
		    ck_pr_load_uint(&locked));
	}

	if (ck_spinlock_locked(&lock) == true) {
		ck_error("ERROR: Lock still owned after join\n");
	}
	fprintf(stderr, "done (passed)\n");

	free(threads);
	return (0);
}

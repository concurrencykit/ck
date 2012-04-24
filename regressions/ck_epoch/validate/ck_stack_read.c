/*
 * Copyright 2010-2012 Samy Al Bahra.
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

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>

#include <ck_backoff.h>
#include <ck_cc.h>
#include <ck_pr.h>
#include <stdbool.h>
#include <stddef.h>
#include <ck_epoch.h>
#include <ck_stack.h>

#include "../../common.h"

static unsigned int threshold;
static unsigned int n_threads;
static unsigned int barrier;
static unsigned int e_barrier;
static unsigned int readers;

#ifndef PAIRS
#define PAIRS 5000000
#endif

#ifndef ITERATE
#define ITERATE 20
#endif

struct node {
	unsigned int value;
	ck_stack_entry_t stack_entry;
	ck_epoch_entry_t epoch_entry;
};
static ck_stack_t stack = CK_STACK_INITIALIZER;
static ck_epoch_t stack_epoch;
CK_STACK_CONTAINER(struct node, stack_entry, stack_container)
CK_EPOCH_CONTAINER(struct node, epoch_entry, epoch_container)
static struct affinity a;

static void
destructor(ck_epoch_entry_t *p)
{
	struct node *e = epoch_container(p);

	free(e);
	return;
}

static void *
read_thread(void *unused CK_CC_UNUSED)
{
	unsigned int j;
	ck_epoch_record_t record;
	ck_stack_entry_t *cursor;

	/*
	 * This is redundant post-incremented in order to silence some
	 * irrelevant GCC warnings. It is volatile in order to prevent
	 * elimination.
	 */
	volatile ck_stack_entry_t *n;

	ck_epoch_register(&stack_epoch, &record);

	if (aff_iterate(&a)) {
		perror("ERROR: failed to affine thread");
		exit(EXIT_FAILURE);
	}

	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) < n_threads);

	while (CK_STACK_ISEMPTY(&stack) == true) {
		if (ck_pr_load_uint(&readers) != 0)
			break;

		ck_pr_stall();
	}

	j = 0;
	for (;;) {
		ck_epoch_read_begin(&record);
		CK_STACK_FOREACH(&stack, cursor) {
			n = cursor;
			j++;
			n++;
		}
		ck_epoch_read_end(&record);

		if (j != 0 && ck_pr_load_uint(&readers) == 0)
			ck_pr_store_uint(&readers, 1);

		if (CK_STACK_ISEMPTY(&stack) == true &&
		    ck_pr_load_uint(&e_barrier) != 0)
			break;
	}

	ck_pr_inc_uint(&e_barrier);
	while (ck_pr_load_uint(&e_barrier) < n_threads);

	fprintf(stderr, "[R] Observed entries: %u\n", j);
	return (NULL);
}

static void *
thread(void *unused CK_CC_UNUSED)
{
	struct node **entry, *e;
	unsigned int i, j;
	ck_epoch_record_t record;
	ck_stack_entry_t *s;

	ck_epoch_register(&stack_epoch, &record);

	if (aff_iterate(&a)) {
		perror("ERROR: failed to affine thread");
		exit(EXIT_FAILURE);
	}

	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) < n_threads);

	entry = malloc(sizeof(struct node *) * PAIRS);
	if (entry == NULL) {
		fprintf(stderr, "Failed allocation.\n");
		exit(EXIT_FAILURE);
	}

	for (j = 0; j < ITERATE; j++) {
		for (i = 0; i < PAIRS; i++) {
			entry[i] = malloc(sizeof(struct node));
			if (entry == NULL) {
				fprintf(stderr, "Failed individual allocation\n");
				exit(EXIT_FAILURE);
			}
		}

		for (i = 0; i < PAIRS; i++) {
			ck_epoch_write_begin(&record);
			ck_stack_push_upmc(&stack, &entry[i]->stack_entry);
			ck_epoch_write_end(&record);
		}

		while (ck_pr_load_uint(&readers) == 0)
			ck_pr_stall();

		for (i = 0; i < PAIRS; i++) {
			ck_epoch_write_begin(&record);
			s = ck_stack_pop_upmc(&stack);
			ck_epoch_write_end(&record);

			e = stack_container(s);
			ck_epoch_free(&record, &e->epoch_entry, destructor);
		}
	}

	ck_pr_inc_uint(&e_barrier);
	while (ck_pr_load_uint(&e_barrier) < n_threads);

	fprintf(stderr, "[W] Peak: %u (%2.2f%%)\n    Reclamations: %" PRIu64 "\n\n",
			record.n_peak,
			(double)record.n_peak / ((double)PAIRS * ITERATE) * 100,
			record.n_reclamations);

	return (NULL);
}

int
main(int argc, char *argv[])
{
	unsigned int i;
	pthread_t *threads;

	if (argc != 4) {
		fprintf(stderr, "Usage: stack <threads> <threshold> <delta>\n");
		exit(EXIT_FAILURE);
	}

	n_threads = atoi(argv[1]);
	threshold = atoi(argv[2]);
	a.delta = atoi(argv[3]);
	a.request = 0;

	threads = malloc(sizeof(pthread_t) * n_threads);

	ck_epoch_init(&stack_epoch, threshold);

	for (i = 0; i < n_threads - 1; i++)
		pthread_create(threads + i, NULL, read_thread, NULL);

	pthread_create(threads + i, NULL, thread, NULL);

	for (i = 0; i < n_threads; i++)
		pthread_join(threads[i], NULL);

	return (0);
}

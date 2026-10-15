/*
 * Copyright 2010-2015 Samy Al Bahra.
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
#include <string.h>
#include <ck_epoch.h>
#include <ck_stack.h>

#include "../../common.h"

static unsigned int n_rd;
static unsigned int n_wr;
static unsigned int n_threads;
static unsigned int barrier;
static unsigned int e_barrier;
static unsigned int readers;
static unsigned int writers;

#ifndef PAIRS_S
#define PAIRS_S 100000
#endif

#ifndef ITERATE_S
#define ITERATE_S 20
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
static const char animate[] = "-/|\\";

static void
destructor(ck_epoch_entry_t *p)
{
	struct node *e = epoch_container(p);

	free(e);
	return;
}

struct poll_object {
	unsigned int destroyed;
	ck_epoch_entry_t epoch_entry;
};
CK_EPOCH_CONTAINER(struct poll_object, epoch_entry, poll_object_container)
CK_STACK_CONTAINER(ck_epoch_entry_t, stack_entry, deferred_container)

static void
poll_object_destroy(ck_epoch_entry_t *p)
{
	struct poll_object *o = poll_object_container(p);

	o->destroyed++;
	return;
}

/*
 * Validates the dispatch arithmetic of ck_epoch_poll with respect to a
 * pinned reader: an object filed under the snapshot epoch e may only be
 * destroyed once every active thread has observed e + 2. The deletion
 * of an object is not required to be visible before the snapshot taken
 * by ck_epoch_call, so a thread which observed e + 1 may still hold a
 * reference to it.
 */
static void
test_poll_arithmetic(void)
{
	ck_epoch_t e;
	ck_epoch_record_t reader, poller;
	struct poll_object o = { 0 };

	ck_epoch_init(&e);
	ck_epoch_register(&e, &reader, NULL);
	ck_epoch_register(&e, &poller, NULL);

	/* The reader pins a section at the current epoch. */
	ck_epoch_begin(&reader, NULL);

	/* The object is filed under the current epoch's slot. */
	ck_epoch_call(&poller, &o.epoch_entry, poll_object_destroy);

	/*
	 * All active threads are at the current epoch: the poller makes
	 * progress and advances the epoch, but the object must survive.
	 */
	if (ck_epoch_poll(&poller) == false) {
		ck_error("ERROR: Poll failed against current-epoch reader.\n");
	}

	if (o.destroyed != 0) {
		ck_error("ERROR: Object destroyed at its call epoch.\n");
	}

	/*
	 * The reader now lags the advanced epoch: the poller must make
	 * no progress and dispatch nothing.
	 */
	if (ck_epoch_poll(&poller) == true) {
		ck_error("ERROR: Poll progressed over a lagging reader.\n");
	}

	if (o.destroyed != 0) {
		ck_error("ERROR: Object destroyed over a lagging reader.\n");
	}

	/* The reader re-enters, having observed e + 1. */
	ck_epoch_end(&reader, NULL);
	ck_epoch_begin(&reader, NULL);

	/*
	 * The object's deletion may have only become visible at e + 1:
	 * the reader may have acquired a reference to it and must be
	 * observed at e + 2 before the object is destroyed.
	 */
	if (ck_epoch_poll(&poller) == false) {
		ck_error("ERROR: Poll failed against current-epoch reader.\n");
	}

	if (o.destroyed != 0) {
		ck_error("ERROR: Object destroyed before every active "
		    "thread observed its deletion epoch.\n");
	}

	/* The reader re-enters, having observed e + 2. */
	ck_epoch_end(&reader, NULL);
	ck_epoch_begin(&reader, NULL);

	if (ck_epoch_poll(&poller) == false) {
		ck_error("ERROR: Poll failed against current-epoch reader.\n");
	}

	if (o.destroyed != 1) {
		ck_error("ERROR: Object not destroyed after every active "
		    "thread observed its deletion epoch + 1.\n");
	}

	ck_epoch_end(&reader, NULL);
	return;
}

/*
 * Validates ck_epoch_observe: a permanently active record which
 * observes the epoch must permit the same dispatch progress as a
 * record cycling sections, with the same timing of destruction.
 */
static void
test_poll_observe(void)
{
	ck_epoch_t e;
	ck_epoch_record_t reader, poller;
	struct poll_object o = { 0 };

	ck_epoch_init(&e);
	ck_epoch_register(&e, &reader, NULL);
	ck_epoch_register(&e, &poller, NULL);

	/* The reader holds a single section for its lifetime. */
	ck_epoch_begin(&reader, NULL);

	ck_epoch_call(&poller, &o.epoch_entry, poll_object_destroy);

	if (ck_epoch_poll(&poller) == false) {
		ck_error("ERROR: Poll failed against observing reader.\n");
	}

	/* The reader lags: no progress is possible. */
	if (ck_epoch_poll(&poller) == true) {
		ck_error("ERROR: Poll progressed over a lagging observer.\n");
	}

	ck_epoch_observe(&reader);

	if (ck_epoch_poll(&poller) == false) {
		ck_error("ERROR: Poll failed against observing reader.\n");
	}

	if (o.destroyed != 0) {
		ck_error("ERROR: Object destroyed before every active "
		    "thread observed its deletion epoch.\n");
	}

	ck_epoch_observe(&reader);

	if (ck_epoch_poll(&poller) == false) {
		ck_error("ERROR: Poll failed against observing reader.\n");
	}

	if (o.destroyed != 1) {
		ck_error("ERROR: Object not destroyed after every active "
		    "thread observed its deletion epoch + 1.\n");
	}

	ck_epoch_end(&reader, NULL);
	return;
}

/*
 * Validates ck_epoch_poll_deferred: dispatched entries must land on the
 * deferred stack exactly once, unexecuted, and the all-inactive path
 * must flush every slot.
 */
static void
test_poll_deferred(void)
{
	ck_epoch_t e;
	ck_epoch_record_t record;
	struct poll_object o = { 0 };
	ck_stack_t deferred = CK_STACK_INITIALIZER;
	ck_stack_entry_t *cursor;
	ck_epoch_entry_t *entry;
	unsigned int n = 0;

	ck_epoch_init(&e);
	ck_epoch_register(&e, &record, NULL);

	ck_epoch_call(&record, &o.epoch_entry, poll_object_destroy);

	/*
	 * No thread is active: the grace period is immediate and every
	 * slot is flushed onto the deferred stack without execution.
	 */
	if (ck_epoch_poll_deferred(&record, &deferred) == false) {
		ck_error("ERROR: Deferred poll failed with no active threads.\n");
	}

	if (o.destroyed != 0) {
		ck_error("ERROR: Deferred entry was executed by poll.\n");
	}

	while ((cursor = ck_stack_pop_npsc(&deferred)) != NULL) {
		entry = deferred_container(cursor);
		entry->function(entry);
		n++;
	}

	if (n != 1 || o.destroyed != 1) {
		ck_error("ERROR: Expected exactly one deferred entry "
		    "(%u dispatched, %u destroyed).\n", n, o.destroyed);
	}

	return;
}

/*
 * Validates ck_epoch_pending and ck_epoch_dispatched: a snapshot stays
 * outstanding until every deferred call it covers has been dispatched,
 * even while calls issued after the snapshot are dispatched ahead of
 * them, and a slot refilled after its dispatch does not revive it.
 */
static void
test_poll_pending(void)
{
	ck_epoch_t e;
	ck_epoch_record_t reader, advancer, record;
	struct poll_object old = { 0 }, young = { 0 }, late = { 0 };
	ck_epoch_pending_t p;
	unsigned int i, reordered = 0;

	ck_epoch_init(&e);
	ck_epoch_register(&e, &reader, NULL);
	ck_epoch_register(&e, &advancer, NULL);
	ck_epoch_register(&e, &record, NULL);

	/* An empty record has nothing outstanding. */
	p = ck_epoch_pending(&record);
	if (ck_epoch_dispatched(&record, &p) == false) {
		ck_error("ERROR: Empty record reported outstanding calls.\n");
	}

	/* The reader pins a section; the old call is filed under e. */
	ck_epoch_begin(&reader, NULL);
	ck_epoch_call(&record, &old.epoch_entry, poll_object_destroy);
	p = ck_epoch_pending(&record);

	if (ck_epoch_dispatched(&record, &p) == true) {
		ck_error("ERROR: Snapshot reported dispatched before any poll.\n");
	}

	/*
	 * Other writers advance the global epoch while this record does
	 * not poll: its old slot is passed over for a full turn.
	 */
	for (i = 0; i < CK_EPOCH_LENGTH - 1; i++) {
		if (ck_epoch_poll(&advancer) == false) {
			ck_error("ERROR: Advancer failed against current-epoch "
			    "reader.\n");
		}

		ck_epoch_end(&reader, NULL);
		ck_epoch_begin(&reader, NULL);
	}

	/* A younger call is filed under the current epoch. */
	ck_epoch_call(&record, &young.epoch_entry, poll_object_destroy);

	/*
	 * Poll with a reader at the current epoch each time.  The young
	 * call is dispatched before the old one; the snapshot must not
	 * report completion until the old one has been dispatched.
	 */
	for (i = 0; i < 2 * CK_EPOCH_LENGTH; i++) {
		if (ck_epoch_poll(&record) == false) {
			ck_error("ERROR: Poll failed against current-epoch "
			    "reader.\n");
		}

		if (young.destroyed != 0 && old.destroyed == 0)
			reordered = 1;

		if (ck_epoch_dispatched(&record, &p) != (old.destroyed != 0)) {
			ck_error("ERROR: Snapshot state %u does not follow the "
			    "old call (destroyed %u, young destroyed %u).\n",
			    p, old.destroyed, young.destroyed);
		}

		if (old.destroyed != 0)
			break;

		ck_epoch_end(&reader, NULL);
		ck_epoch_begin(&reader, NULL);
	}

	if (old.destroyed != 1 || young.destroyed != 1) {
		ck_error("ERROR: Calls not dispatched (old %u, young %u).\n",
		    old.destroyed, young.destroyed);
	}

	if (reordered == 0) {
		ck_error("ERROR: Young call was not dispatched ahead of the old "
		    "one; test did not exercise reordering.\n");
	}

	/* A call filed after completion does not revive the snapshot. */
	ck_epoch_call(&record, &late.epoch_entry, poll_object_destroy);
	if (ck_epoch_dispatched(&record, &p) == false) {
		ck_error("ERROR: Completed snapshot revived by a later call.\n");
	}

	/* With no active thread, a poll flushes every slot. */
	ck_epoch_end(&reader, NULL);
	p = ck_epoch_pending(&record);
	if (ck_epoch_poll(&record) == false || late.destroyed != 1) {
		ck_error("ERROR: Poll failed to flush with no active threads.\n");
	}

	if (ck_epoch_dispatched(&record, &p) == false) {
		ck_error("ERROR: Snapshot outstanding after a full flush.\n");
	}

	return;
}

static void *
read_thread(void *unused CK_CC_UNUSED)
{
	unsigned int j;
	ck_epoch_record_t *record CK_CC_CACHELINE;
	ck_stack_entry_t *cursor, *n;

	record = common_aligned_alloc(common_alignof(ck_epoch_record_t),
	    sizeof(ck_epoch_record_t));
	if (record == NULL)
		ck_error("record allocation failure");

	ck_epoch_register(&stack_epoch, record, NULL);

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
		ck_epoch_begin(record, NULL);
		CK_STACK_FOREACH(&stack, cursor) {
			if (cursor == NULL)
				continue;

			n = CK_STACK_NEXT(cursor);
			j += ck_pr_load_ptr(&n) != NULL;
		}
		ck_epoch_end(record, NULL);

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
write_thread(void *unused CK_CC_UNUSED)
{
	struct node **entry, *e;
	unsigned int i, j, tid;
	ck_epoch_record_t *record;
	ck_stack_entry_t *s;

	record = common_aligned_alloc(common_alignof(ck_epoch_record_t),
	    sizeof(ck_epoch_record_t));
	if (record == NULL)
		ck_error("record allocation failure");
	ck_epoch_register(&stack_epoch, record, NULL);

	if (aff_iterate(&a)) {
		perror("ERROR: failed to affine thread");
		exit(EXIT_FAILURE);
	}

	tid = ck_pr_faa_uint(&writers, 1);
	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) < n_threads);

	entry = malloc(sizeof(struct node *) * PAIRS_S);
	if (entry == NULL) {
		ck_error("Failed allocation.\n");
	}

	for (j = 0; j < ITERATE_S; j++) {
		for (i = 0; i < PAIRS_S; i++) {
			entry[i] = malloc(sizeof(struct node));
			if (entry == NULL) {
				ck_error("Failed individual allocation\n");
			}
		}

		for (i = 0; i < PAIRS_S; i++) {
			ck_stack_push_upmc(&stack, &entry[i]->stack_entry);
		}

		while (ck_pr_load_uint(&readers) == 0)
			ck_pr_stall();

		if (tid == 0) {
			fprintf(stderr, "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b[W] %2.2f: %c",
			    (double)j / ITERATE_S, animate[i % strlen(animate)]);
		}

		for (i = 0; i < PAIRS_S; i++) {
			/*
			 * Concurrent writers each advance the epoch through
			 * ck_epoch_poll, so the deferral must be made within
			 * the section that removed the object (see
			 * ck_epoch_call).
			 */
			ck_epoch_begin(record, NULL);
			s = ck_stack_pop_upmc(&stack);
			e = stack_container(s);
			ck_epoch_call(record, &e->epoch_entry, destructor);
			ck_epoch_end(record, NULL);

			ck_epoch_poll(record);
		}
	}

	ck_epoch_barrier(record);

	if (tid == 0) {
		fprintf(stderr, "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b[W] Peak: %u (%2.2f%%)\n    Reclamations: %u\n\n",
			record->n_peak,
			(double)record->n_peak / ((double)PAIRS_S * ITERATE_S) * 100,
			record->n_dispatch);
	}

	ck_pr_inc_uint(&e_barrier);
	while (ck_pr_load_uint(&e_barrier) < n_threads);
	return (NULL);
}

int
main(int argc, char *argv[])
{
	unsigned int i;
	pthread_t *threads;

	if (argc != 4) {
		ck_error("Usage: stack <#readers> <#writers> <affinity delta>\n");
	}

	test_poll_arithmetic();
	test_poll_observe();
	test_poll_deferred();
	test_poll_pending();

	n_rd = atoi(argv[1]);
	n_wr = atoi(argv[2]);
	n_threads = n_wr + n_rd;

	a.delta = atoi(argv[3]);
	a.request = 0;

	threads = malloc(sizeof(pthread_t) * n_threads);
	ck_epoch_init(&stack_epoch);

	for (i = 0; i < n_rd; i++)
		pthread_create(threads + i, NULL, read_thread, NULL);

	do {
		pthread_create(threads + i, NULL, write_thread, NULL);
	} while (++i < n_wr + n_rd);

	for (i = 0; i < n_threads; i++)
		pthread_join(threads[i], NULL);

	return (0);
}

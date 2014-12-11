/*
 * Copyright 2011-2014 Samy Al Bahra.
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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <ck_barrier.h>
#include <ck_ring.h>
#include <ck_spinlock.h>
#include "../../common.h"

#ifndef ITERATIONS
#define ITERATIONS 128
#endif

struct context {
	unsigned int tid;
	unsigned int previous;
	unsigned int next;
	struct entry **buffer;
};

struct entry {
	unsigned long value_long;
	unsigned int magic;
	unsigned int ref;
	int tid;
	int value;
};

CK_RING_PROTOTYPE(entry, entry *)

static int nthr;
static ck_ring_t *ring;
static ck_ring_t ring_spmc CK_CC_CACHELINE;
static struct affinity a;
static int size;
static int eb;
static ck_barrier_centralized_t barrier = CK_BARRIER_CENTRALIZED_INITIALIZER;
static struct context *_context;

static void *
test_spmc(void *c)
{
	unsigned int observed = 0;
	unsigned long previous = 0;
	unsigned int seed;
	int i, k, j, tid;
	struct context *context = c;
	struct entry **buffer;

	buffer = context->buffer;
        if (aff_iterate(&a)) {
                perror("ERROR: Could not affine thread");
                exit(EXIT_FAILURE);
        }

	tid = ck_pr_faa_int(&eb, 1);
	ck_pr_fence_memory();
	while (ck_pr_load_int(&eb) != nthr - 1);

	for (i = 0; i < ITERATIONS; i++) {
		for (j = 0; j < size; j++) {
			struct entry *o;
			int spin;

			/* Keep trying until we encounter at least one node. */
			if (j & 1) {
				while (CK_RING_DEQUEUE_SPMC(entry,
				    &ring_spmc, buffer, &o) == false);
			} else {
				while (CK_RING_TRYDEQUEUE_SPMC(entry,
				    &ring_spmc, buffer, &o) == false);
			}

			observed++;
			if (o->value < 0
			    || o->value != o->tid
			    || o->magic != 0xdead
			    || (previous != 0 && previous >= o->value_long)) {
				ck_error("[0x%p] (%x) (%d, %d) >< (0, %d)\n",
					(void *)o, o->magic, o->tid, o->value, size);
			}

			o->magic = 0xbeef;
			o->value = -31337;
			o->tid = -31338;
			previous = o->value_long;

			if (ck_pr_faa_uint(&o->ref, 1) != 0) {
				ck_error("[%p] We dequeued twice.\n", (void *)o);
			}

			if ((i % 4) == 0) {
				spin = common_rand_r(&seed) % 16384;
				for (k = 0; k < spin; k++) {
					ck_pr_stall();
				}
			}

			free(o);
		}
	}

	fprintf(stderr, "[%d] Observed %u\n", tid, observed);
	return NULL;
}

static void *
test(void *c)
{
	struct context *context = c;
	struct entry *entry;
	unsigned int s;
	int i, j;
	bool r;
	struct entry **buffer = context->buffer;
	ck_barrier_centralized_state_t sense =
	    CK_BARRIER_CENTRALIZED_STATE_INITIALIZER;

        if (aff_iterate(&a)) {
                perror("ERROR: Could not affine thread");
                exit(EXIT_FAILURE);
        }

	if (context->tid == 0) {
		struct entry **entries;

		entries = malloc(sizeof(struct entry *) * size);
		assert(entries != NULL);

		if (ck_ring_size(ring) != 0) {
			ck_error("More entries than expected: %u > 0\n",
				ck_ring_size(ring));
		}

		for (i = 0; i < size; i++) {
			entries[i] = malloc(sizeof(struct entry));
			assert(entries[i] != NULL);

			entries[i]->value = i;
			entries[i]->tid = 0;

			if (i & 1) {
				r = CK_RING_ENQUEUE_SPMC(entry, ring, buffer,
				    &entries[i]);
			} else {
				r = CK_RING_ENQUEUE_SPMC_SIZE(entry, ring,
					buffer, &entries[i], &s);

				if ((int)s != i) {
					ck_error("Size is %u, expected %d.\n",
					    s, size);
				}
			}

			assert(r != false);
		}

		if (ck_ring_size(ring) != (unsigned int)size) {
			ck_error("Less entries than expected: %u < %d\n",
				ck_ring_size(ring), size);
		}

		if (ck_ring_capacity(ring) != ck_ring_size(ring) + 1) {
			ck_error("Capacity less than expected: %u < %u\n",
				ck_ring_size(ring), ck_ring_capacity(ring));
		}
	}

	/*
	 * Wait for all threads. The idea here is to maximize the contention.
	 */
	ck_barrier_centralized(&barrier, &sense, nthr);

	for (i = 0; i < ITERATIONS; i++) {
		for (j = 0; j < size; j++) {
			buffer = _context[context->previous].buffer;
			while (CK_RING_DEQUEUE_SPMC(entry,
			    ring + context->previous,
			    buffer, &entry) == false);

			if (context->previous != (unsigned int)entry->tid) {
				ck_error("[%u:%p] %u != %u\n",
				    context->tid, (void *)entry,
				    entry->tid, context->previous);
			}

			if (entry->value < 0 || entry->value >= size) {
				ck_error("[%u:%p] %u </> %u\n",
				    context->tid, (void *)entry,
				    entry->tid, context->previous);
			}

			entry->tid = context->tid;
			buffer = context->buffer;

			if (i & 1) {
				r = CK_RING_ENQUEUE_SPMC(entry,
				    ring + context->tid,
				    buffer, &entry);
			} else {
				r = CK_RING_ENQUEUE_SPMC_SIZE(entry,
				    ring + context->tid,
				    buffer, &entry, &s);

				if ((int)s >= size) {
					ck_error("Size %u out of range of %d\n",
					    s, size);
				}
			}
			assert(r == true);
		}
	}

	return NULL;
}

int
main(int argc, char *argv[])
{
	int i, r;
	unsigned long l;
	pthread_t *thread;
	struct entry **buffer;

	if (argc != 4) {
		ck_error("Usage: validate <threads> <affinity delta> <size>\n");
	}

	a.request = 0;
	a.delta = atoi(argv[2]);

	nthr = atoi(argv[1]);
	assert(nthr >= 1);

	size = atoi(argv[3]);
	assert(size >= 4 && (size & size - 1) == 0);
	size -= 1;

	ring = malloc(sizeof(ck_ring_t) * nthr);
	assert(ring);

	_context = malloc(sizeof(*_context) * nthr);
	assert(_context);

	thread = malloc(sizeof(pthread_t) * nthr);
	assert(thread);

	fprintf(stderr, "SPSC test:");
	for (i = 0; i < nthr; i++) {
		_context[i].tid = i;
		if (i == 0) {
			_context[i].previous = nthr - 1;
			_context[i].next = i + 1;
		} else if (i == nthr - 1) {
			_context[i].next = 0;
			_context[i].previous = i - 1;
		} else {
			_context[i].next = i + 1;
			_context[i].previous = i - 1;
		}

		buffer = malloc(sizeof(struct entry *) * (size + 1));
		assert(buffer);
		memset(buffer, 0, sizeof(struct entry *) * (size + 1));
		_context[i].buffer = buffer;
		ck_ring_init(ring + i, size + 1);
		r = pthread_create(thread + i, NULL, test, _context + i);
		assert(r == 0);
	}

	for (i = 0; i < nthr; i++)
		pthread_join(thread[i], NULL);

	fprintf(stderr, " done\n");

	fprintf(stderr, "SPMC test:\n");
	buffer = malloc(sizeof(struct entry *) * (size + 1));
	assert(buffer);
	memset(buffer, 0, sizeof(struct entry *) * (size + 1));
	ck_ring_init(&ring_spmc, size + 1);
	for (i = 0; i < nthr - 1; i++) {
		_context[i].buffer = buffer;
		r = pthread_create(thread + i, NULL, test_spmc, _context + i);
		assert(r == 0);
	}

	for (l = 0; l < (unsigned long)size * ITERATIONS * (nthr - 1) ; l++) {
		struct entry *entry = malloc(sizeof *entry);

		assert(entry != NULL);
		entry->value_long = l;
		entry->value = (int)l;
		entry->tid = (int)l;
		entry->magic = 0xdead;
		entry->ref = 0;

		/* Wait until queue is not full. */
		if (l & 1) {
			while (CK_RING_ENQUEUE_SPMC(entry, &ring_spmc,
			    buffer, &entry) == false) {
				ck_pr_stall();
			}
		} else {
			unsigned int s;

			while (CK_RING_ENQUEUE_SPMC_SIZE(entry, &ring_spmc,
			    buffer, &entry, &s) == false) {
				ck_pr_stall();
			}

			if ((int)s >= (size * ITERATIONS * (nthr - 1))) {
				ck_error("MPMC: Unexpected size of %u\n", s);
			}
		}
	}

	for (i = 0; i < nthr - 1; i++)
		pthread_join(thread[i], NULL);

	return 0;
}


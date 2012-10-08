/*
 * Copyright 2011-2012 Samy Al Bahra.
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
#include <pthread.h>

#include <ck_ring.h>
#include "../../common.h"

#ifndef ITERATIONS
#define ITERATIONS 128
#endif

struct context {
	unsigned int tid;
	unsigned int previous;
	unsigned int next;
};

struct entry {
	int tid;
	int value;
};

static int nthr;
static ck_ring_t *ring;
static struct affinity a;
static int size;
static volatile int barrier;

static void *
test(void *c)
{
	struct context *context = c;
	struct entry *entry;
	int i, j;
	bool r;

        if (aff_iterate(&a)) {
                perror("ERROR: Could not affine thread");
                exit(EXIT_FAILURE);
        }

	if (context->tid == 0) {
		struct entry *entries;

		entries = malloc(sizeof(struct entry) * size);
		assert(entries != NULL);

		if (ck_ring_size(ring) != 0) {
			ck_error("More entries than expected: %u > 0\n",
				ck_ring_size(ring));
		}

		for (i = 0; i < size; i++) {
			entries[i].value = i;
			entries[i].tid = 0;

			r = ck_ring_enqueue_spsc(ring, entries + i);
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

		barrier = 1;
	}

	while (barrier == 0);

	for (i = 0; i < ITERATIONS; i++) {
		for (j = 0; j < size; j++) {
			while (ck_ring_dequeue_spsc(ring + context->previous, &entry) == false);

			if (context->previous != (unsigned int)entry->tid) {
				ck_error("[%u:%p] %u != %u\n",
					context->tid, (void *)entry, entry->tid, context->previous);
			}

			if (entry->value != j) {
				ck_error("[%u:%p] %u != %u\n",
					context->tid, (void *)entry, entry->tid, context->previous);
			}

			entry->tid = context->tid;
			r = ck_ring_enqueue_spsc(ring + context->tid, entry);
			assert(r == true);
		}
	}

	return NULL;
}

int
main(int argc, char *argv[])
{
	int i, r;
	void *buffer;
	struct context *context;
	pthread_t *thread;

	if (argc != 4) {
		ck_error("Usage: validate <threads> <affinity delta> <size>\n");
	}

	a.request = 0;
	a.delta = atoi(argv[2]);

	nthr = atoi(argv[1]);
	assert(nthr >= 1);

	size = atoi(argv[3]);
	assert(size > 4 && (size & size - 1) == 0);
	size -= 1;

	ring = malloc(sizeof(ck_ring_t) * nthr);
	assert(ring);

	context = malloc(sizeof(*context) * nthr);
	assert(context);

	thread = malloc(sizeof(pthread_t) * nthr);
	assert(thread);

	for (i = 0; i < nthr; i++) {
		context[i].tid = i;
		if (i == 0) {
			context[i].previous = nthr - 1;
			context[i].next = i + 1;
		} else if (i == nthr - 1) {
			context[i].next = 0;
			context[i].previous = i - 1;
		} else {
			context[i].next = i + 1;
			context[i].previous = i - 1;
		}

		buffer = malloc(sizeof(void *) * (size + 1));
		assert(buffer);
		ck_ring_init(ring + i, buffer, size + 1);
		r = pthread_create(thread + i, NULL, test, context + i);
		assert(r == 0);
	}

	for (i = 0; i < nthr; i++)
		pthread_join(thread[i], NULL);

	return (0);
}

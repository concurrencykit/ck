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
CK_RING(entry, entry_ring)

static int nthr;
static CK_RING_INSTANCE(entry_ring) *ring;
static struct affinity a;
static int size;
static volatile int barrier;

static void *
test(void *c)
{
	struct context *context = c;
	struct entry entry;
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

		if (CK_RING_SIZE(entry_ring, ring) != 0) {
			fprintf(stderr, "Ring should be empty: %u\n",
				CK_RING_SIZE(entry_ring, ring));
			exit(EXIT_FAILURE);
		}

		for (i = 0; i < size; i++) {
			entries[i].value = i;
			entries[i].tid = 0;

			r = CK_RING_ENQUEUE_SPSC(entry_ring, ring, entries + i);
		}

		if (CK_RING_SIZE(entry_ring, ring) !=
		    CK_RING_CAPACITY(entry_ring, ring) - 1) {
			fprintf(stderr, "Ring has incorrect size or capacity: %u != %u\n",
				CK_RING_SIZE(entry_ring, ring),
				CK_RING_CAPACITY(entry_ring, ring));
			exit(EXIT_FAILURE);
		}

		barrier = 1;
	}

	while (barrier == 0);

	for (i = 0; i < ITERATIONS; i++) {
		for (j = 0; j < size; j++) {
			while (CK_RING_DEQUEUE_SPSC(entry_ring, ring + context->previous, &entry) == false);

			if (context->previous != (unsigned int)entry.tid) {
				fprintf(stderr, "[%u] %u != %u\n",
					context->tid, entry.tid, context->previous);
				exit(EXIT_FAILURE);
			}

			if (entry.value != j) {
				fprintf(stderr, "[%u] %u != %u\n",
					context->tid, entry.tid, context->previous);
				exit(EXIT_FAILURE);
			}

			entry.tid = context->tid;
			r = CK_RING_ENQUEUE_SPSC(entry_ring, ring + context->tid, &entry);
			assert(r == true);
		}
	}

	return NULL;
}

int
main(int argc, char *argv[])
{
	int i, r;
	struct entry *buffer;
	struct context *context;
	pthread_t *thread;

	if (argc != 4) {
		fprintf(stderr, "Usage: validate <threads> <affinity delta> <size>\n");
		exit(EXIT_FAILURE);
	}

	a.request = 0;
	a.delta = atoi(argv[2]);

	nthr = atoi(argv[1]);
	assert(nthr >= 1);

	size = atoi(argv[3]);
	assert(size > 4 && (size & size - 1) == 0);
	size -= 1;

	ring = malloc(sizeof(CK_RING_INSTANCE(entry_ring)) * nthr);
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

		buffer = malloc(sizeof(struct entry) * (size + 1));
		assert(buffer);
		CK_RING_INIT(entry_ring, ring + i, buffer, size + 1);
		r = pthread_create(thread + i, NULL, test, context + i);
		assert(r == 0);
	}

	for (i = 0; i < nthr; i++)
		pthread_join(thread[i], NULL);

	return (0);
}

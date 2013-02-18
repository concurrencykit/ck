/*
 * Copyright 2012-2013 Abel P. Mathew
 * Copyright 2012-2013 Samy Al Bahra
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
#include <unistd.h>
#include <ck_bag.h>
#include <ck_epoch.h>

#include "../../common.h"

#define NUM_READER_THREADS 2
#define READ_LATENCY 8

static ck_bag_t bag;
static ck_epoch_t epoch_bag;
static ck_epoch_record_t epoch_wr;
static int leave;
static unsigned int barrier;
static unsigned int writer_max = 32768;

struct bag_epoch {
	ck_epoch_entry_t epoch_entry;
};

static void
bag_destroy(ck_epoch_entry_t *e)
{

	free(e);
	return;
}

static void *
bag_malloc(size_t r)
{
	struct bag_epoch *b;

	b = malloc(sizeof(*b) + r);
	return b + 1;
}

static void
bag_free(void *p, size_t b, bool r)
{
	struct bag_epoch *e = p;

	(void)b;

	if (r == true) {
		ck_epoch_call(&epoch_bag, &epoch_wr, &(--e)->epoch_entry, bag_destroy);
	} else {
		free(--e);
	}

	return;
}

static struct ck_malloc allocator = {
	.malloc = bag_malloc,
	.free = bag_free
};

static void *
reader(void *arg)
{
	void *curr_ptr;
	intptr_t curr, prev, curr_max, prev_max;
	unsigned long long n_entries = 0, iterations = 0;
	ck_epoch_record_t epoch_record;
	ck_bag_iterator_t iterator;
	struct ck_bag_block *block = NULL;

	(void)arg;

	ck_epoch_register(&epoch_bag, &epoch_record);

	/*
	 * Check if entries within a block are sequential. Since ck_bag inserts
	 * newly occupied blocks at the beginning of the list, there is no ordering
	 * guarantee across the bag.
	 */
	for (;;) {
		ck_epoch_begin(&epoch_bag, &epoch_record);
		ck_bag_iterator_init(&iterator, &bag);
		curr_max = prev_max = prev = -1;
		block = NULL;

		while (ck_bag_next(&iterator, &curr_ptr) == true) {
			if (block != iterator.block) {
				prev = -1;
				curr = 0;
				prev_max = curr_max;
				curr_max = 0;
				block = iterator.block;
			}

			curr = (uintptr_t)(curr_ptr);
			if (curr < prev) {
				/* Ascending order within block violated */
				ck_error("%p: %p: %ju\nERROR: %ju < %ju\n",
					(void *)&epoch_record, (void *)iterator.block, (uintmax_t)curr, (uintmax_t)curr, (uintmax_t)prev);
			} else if (prev_max != -1 && curr > prev_max) {
				/* Max of prev block > max of current block */
				ck_error("%p: %p: %ju\nERROR: %ju > %ju\n",
					(void *)&epoch_record, (void *)iterator.block, (uintmax_t)curr, (uintmax_t)curr, (uintmax_t)prev_max);
			}

			curr_max = curr;

			prev = curr;
			n_entries++;
		}
		ck_epoch_end(&epoch_bag, &epoch_record);

		iterations++;
		if (ck_pr_load_int(&leave) == 1)
			break;
	}

	fprintf(stderr, "Read %llu entries in %llu iterations.\n", n_entries, iterations);

	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) != NUM_READER_THREADS + 1)
		ck_pr_stall();

	return NULL;

}

static void *
writer_thread(void *unused)
{
	unsigned int i;
	unsigned int iteration = 0;

	(void)unused;

	for (;;) {
		iteration++;
		for (i = 1; i <= writer_max; i++) {
			if (ck_bag_put_spmc(&bag, (void *)(uintptr_t)i) == false) {
				perror("ck_bag_put_spmc");
				exit(EXIT_FAILURE);
			}

			if (ck_bag_member_spmc(&bag, (void *)(uintptr_t)i) == false) {
				ck_error("ck_bag_put_spmc [%u]: %u\n", iteration, i);
			}
		}

		if (ck_pr_load_int(&leave) == 1)
			break;

		for (i = 1; i < writer_max; i++) {
			void *replace = (void *)(uintptr_t)i;
			if (ck_bag_set_spmc(&bag, (void *)(uintptr_t)i, replace) == false) {
				ck_error("ERROR: set %ju != %ju",
						(uintmax_t)(uintptr_t)replace, (uintmax_t)i);
			}
		}

		for (i = writer_max; i > 0; i--) {
			if (ck_bag_member_spmc(&bag, (void *)(uintptr_t)i) == false) {
				ck_error("ck_bag_member_spmc [%u]: %u\n", iteration, i);
			}

			if (ck_bag_remove_spmc(&bag, (void *)(uintptr_t)i) == false) {
				ck_error("ck_bag_remove_spmc [%u]: %u\n", iteration, i);
			}
		}

		ck_epoch_poll(&epoch_bag, &epoch_wr);
	}

	fprintf(stderr, "Writer %u iterations, %u writes per iteration.\n", iteration, writer_max);
	while (ck_pr_load_uint(&barrier) != NUM_READER_THREADS)
		ck_pr_stall();

	ck_pr_inc_uint(&barrier);
	return NULL;
}

int
main(int argc, char **argv)
{
	pthread_t *readers;
	pthread_t writer;
	unsigned int i, curr;
	void *curr_ptr;
	ck_bag_iterator_t bag_it;
	size_t b = CK_BAG_DEFAULT;

	if (argc >= 2) {
		int r = atoi(argv[1]);
		if (r <= 0) {
			ck_error("# entries in block must be > 0\n");
		}

		b = (size_t)r;
	}

	if (argc >= 3) {
		int r = atoi(argv[2]);
		if (r < 16) {
			ck_error("# entries must be >= 16\n");
		}

		writer_max = (unsigned int)r;
	}

	ck_epoch_init(&epoch_bag);
	ck_epoch_register(&epoch_bag, &epoch_wr);
	ck_bag_allocator_set(&allocator, sizeof(struct bag_epoch));
	if (ck_bag_init(&bag, b, CK_BAG_ALLOCATE_GEOMETRIC) == false) {
		ck_error("Error: failed ck_bag_init().");
	}
	fprintf(stderr, "Configuration: %u entries, %zu bytes/block, %zu entries/block\n", writer_max, bag.info.bytes, bag.info.max);

	i = 1;
	/* Basic Test */
	ck_bag_put_spmc(&bag, (void *)(uintptr_t)i);
	ck_bag_remove_spmc(&bag, (void *)(uintptr_t)i);
	ck_bag_put_spmc(&bag, (void *)(uintptr_t)i);

	/* Sequential test */
	for (i = 1; i <= 10; i++)
		ck_bag_put_spmc(&bag, (void *)(uintptr_t)i);

	for (i = 1; i <= 10; i++)
		ck_bag_remove_spmc(&bag, (void *)(uintptr_t)i);

	for (i = 10; i > 0; i--)
		ck_bag_remove_spmc(&bag, (void *)(uintptr_t)i);

	for (i = 1; i <= 10; i++)
		ck_bag_put_spmc(&bag, (void *)(uintptr_t)i);

	ck_bag_iterator_init(&bag_it, &bag);
	while (ck_bag_next(&bag_it, &curr_ptr)) {
		curr = (uintptr_t)(curr_ptr);
		if (curr > (uintptr_t)i)
			ck_error("ERROR: %ju != %u\n", (uintmax_t)curr, i);

		ck_bag_remove_spmc(&bag, curr_ptr);
	}

	/* Concurrent test */
	pthread_create(&writer, NULL, writer_thread, NULL);
	readers = malloc(sizeof(pthread_t) * NUM_READER_THREADS);
	for (i = 0; i < NUM_READER_THREADS; i++) {
		pthread_create(&readers[i], NULL, reader, NULL);
	}

	fprintf(stderr, "Waiting...");
	common_sleep(30);
	fprintf(stderr, "done\n");

	ck_pr_store_int(&leave, 1);
	for (i = 0; i < NUM_READER_THREADS; i++)
		pthread_join(readers[i], NULL);

	pthread_join(writer, NULL);
	fprintf(stderr, "Current entries: %u\n", ck_bag_count(&bag));
	ck_bag_destroy(&bag);
	return 0;
}


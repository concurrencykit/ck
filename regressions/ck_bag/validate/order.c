/*
 * Copyright 2012 Abel P. Mathew
 * Copyright 2012 Samy Al Bahra
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
#include <ck_bag.h>
#include <ck_epoch.h>

#include "../../common.h"

#define NUM_READER_THREADS 8
#define WRITER_MAX (1 << 17)
#define READ_LATENCY 8

static ck_bag_t bag;
static int leave;
static unsigned int barrier;

static void *
bag_malloc(size_t r)
{

	return malloc(r);
}

static void
bag_free(void *p, size_t b, bool r)
{

	(void)p;
	(void)b;
	(void)r;
//	free(p);
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

	(void)arg;

	ck_bag_iterator_t iterator;
	struct ck_bag_block *block = NULL;

	/*
	 * Check if entries within a block are sequential. Since ck_bag inserts
	 * newly occupied blocks at the beginning of the list, there is no ordering
	 * guarantee across the bag.
	 */
	for (;;) {
		ck_bag_iterator_init(&iterator, &bag);
		curr_max = prev_max = prev = -1;

		while (ck_bag_next(&iterator, &curr_ptr)) {

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
				fprintf(stderr, "ERROR: %ju < %ju \n",
				    (uintmax_t)curr, (uintmax_t)prev);
				exit(EXIT_FAILURE);
			} else if (prev_max != -1 && curr > prev_max) {
				/* Max of prev block > max of current block */
				fprintf(stderr, "ERROR: %ju > prev_max: %ju\n",
				    (uintmax_t)curr, (uintmax_t)prev_max);
				exit(EXIT_FAILURE);
			}

			curr_max = curr;

			prev = curr;
			n_entries++;
		}

		iterations++;
		if (ck_pr_load_int(&leave) == 1)
			break;
	}

	fprintf(stderr, "Read %llu entries in %llu iterations.\n", n_entries, iterations);

	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) != NUM_READER_THREADS)
		ck_pr_stall();

	return NULL;

}

static void *
writer_thread(void *unused)
{
	unsigned int i;

	(void)unused;

	for (;;) {
		for (i = 0; i < WRITER_MAX; i++)
			ck_bag_put_spmc(&bag, (void *)(uintptr_t)i);

		if (ck_pr_load_int(&leave) == 1)
			break;

		for (i = 0; i < WRITER_MAX >> 1; i++) {
			void *replace = (void *)(uintptr_t)i;
			if (ck_bag_set_spmc(&bag, (void *)(uintptr_t)i, replace) == false ||
					replace != (void *)(uintptr_t)i) {
				fprintf(stderr, "ERROR: set %ju != %ju",
						(uintmax_t)(uintptr_t)replace, (uintmax_t)i);
				exit(EXIT_FAILURE);
			}
		}

		if (ck_pr_load_int(&leave) == 1)
			break;

		for (i = WRITER_MAX; i > 1; i--)
			ck_bag_remove_spmc(&bag, (void *)((uintptr_t)i - 1));
	}

	while (ck_pr_load_uint(&barrier) != NUM_READER_THREADS)
		ck_pr_stall();

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

	(void)argc;
	(void)argv;

	ck_bag_allocator_set(&allocator, 0);
	ck_bag_init(&bag, CK_BAG_DEFAULT, CK_BAG_ALLOCATE_GEOMETRIC);

	fprintf(stderr, "Block Size: %zuB\n", bag.info.bytes);

	/* Sequential test */
	for (i = 0; i < 10; i++)
		ck_bag_put_spmc(&bag, (void *)(uintptr_t)i);

	ck_bag_iterator_init(&bag_it, &bag);
	while (ck_bag_next(&bag_it, &curr_ptr)) {
		curr = (uintptr_t)(curr_ptr);
		if (curr > (uintptr_t)i)
			fprintf(stderr, "ERROR: %ju != %u", (uintmax_t)curr, i);

		ck_bag_remove_spmc(&bag, curr_ptr);
	}

	/* Concurrent test */
	pthread_create(&writer, NULL, writer_thread, NULL);
	readers = malloc(sizeof(pthread_t) * NUM_READER_THREADS);
	for (i = 0; i < NUM_READER_THREADS; i++) {
		pthread_create(&readers[i], NULL, reader, NULL);
	}

	sleep(20);

	ck_pr_store_int(&leave, 1);
	for (i = 0; i < NUM_READER_THREADS; i++)
		pthread_join(readers[i], NULL);

	pthread_join(writer, NULL);
	fprintf(stderr, "Current Entries: %u\n", ck_bag_count(&bag));
	ck_bag_destroy(&bag);
	return 0;
}


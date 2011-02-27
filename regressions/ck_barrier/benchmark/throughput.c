/*
 * Copyright 2011 David Joseph.
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

#include <pthread.h>
#include <unistd.h>
#include <ck_stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <ck_pr.h>
#include <ck_barrier.h>

#include "../../common.h"

static int done = 0;
static unsigned int count = 0;
static struct affinity a;
static int nthr;
static ck_barrier_centralized_t barrier = CK_BARRIER_CENTRALIZED_INITIALIZER;

static void *
thread(void *null CK_CC_UNUSED)
{
	ck_barrier_centralized_state_t state = CK_BARRIER_CENTRALIZED_STATE_INITIALIZER;
	unsigned int counter = 0;

	aff_iterate(&a);

	while (ck_pr_load_int(&done) == 0) {
		ck_barrier_centralized(&barrier, &state, nthr);
		++counter;
		ck_barrier_centralized(&barrier, &state, nthr);
		++counter;
		ck_barrier_centralized(&barrier, &state, nthr);
		++counter;
		ck_barrier_centralized(&barrier, &state, nthr);
		++counter;
		ck_barrier_centralized(&barrier, &state, nthr);
		++counter;
		ck_barrier_centralized(&barrier, &state, nthr);
		++counter;
		ck_barrier_centralized(&barrier, &state, nthr);
		++counter;
		ck_barrier_centralized(&barrier, &state, nthr);
		++counter;
	}

	ck_pr_add_uint(&count, counter);
	return (NULL);
}

int
main(int argc, char *argv[])
{
	pthread_t *threads;
	int i;

	if (argc != 3) {
		fprintf(stderr, "Correct usage: <number of threads> <affinity delta>\n");
		exit(EXIT_FAILURE);
	}

	nthr = atoi(argv[1]);
        if (nthr <= 0) { 
                fprintf(stderr, "ERROR: Number of threads must be greater than 0\n"); 
                exit(EXIT_FAILURE); 
        } 
 
        threads = malloc(sizeof(pthread_t) * nthr); 
        if (threads == NULL) { 
                fprintf(stderr, "ERROR: Could not allocate thread structures\n"); 
                exit(EXIT_FAILURE); 
        } 
 
        a.delta = atoi(argv[2]); 
 
        fprintf(stderr, "Creating threads (barrier)..."); 
        for (i = 0; i < nthr; ++i) { 
                if (pthread_create(&threads[i], NULL, thread, NULL)) { 
                        fprintf(stderr, "ERROR: Could not create thread %d\n", i); 
                        exit(EXIT_FAILURE); 
                } 
        } 
        fprintf(stderr, "done\n"); 

	sleep(10);

	ck_pr_store_int(&done, 1);
	for (i = 0; i < nthr; ++i) 
		pthread_join(threads[i], NULL);
	printf("%d %16" PRIu64 "\n", nthr, count);

	return (0);
}


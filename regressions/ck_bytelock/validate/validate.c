#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>

#include <ck_pr.h>
#include <ck_bytelock.h>

#include "../../common.h"

#ifndef ITERATE
#define ITERATE 5000000
#endif

struct block {
	unsigned int tid;
};

static struct affinity a;
static unsigned int locked = 0;
static int nthr;
static ck_bytelock_t lock = CK_BYTELOCK_INITIALIZER;

static void *
thread(void *null)
{
	struct block *context = null;
	int i = ITERATE;
	unsigned int l;

        if (aff_iterate(&a)) {
                perror("ERROR: Could not affine thread");
                exit(EXIT_FAILURE);
        }

	while (i--) {
		ck_bytelock_write_lock(&lock, context->tid);
		{
			l = ck_pr_load_uint(&locked);
			if (l != 0) {
				fprintf(stderr, "ERROR [WR:%d]: %u != 0\n", __LINE__, l);
				exit(EXIT_FAILURE);
			}

			ck_pr_inc_uint(&locked);
			ck_pr_inc_uint(&locked);
			ck_pr_inc_uint(&locked);
			ck_pr_inc_uint(&locked);
			ck_pr_inc_uint(&locked);
			ck_pr_inc_uint(&locked);
			ck_pr_inc_uint(&locked);
			ck_pr_inc_uint(&locked);

			l = ck_pr_load_uint(&locked);
			if (l != 8) {
				fprintf(stderr, "ERROR [WR:%d]: %u != 2\n", __LINE__, l);
				exit(EXIT_FAILURE);
			}

			ck_pr_dec_uint(&locked);
			ck_pr_dec_uint(&locked);
			ck_pr_dec_uint(&locked);
			ck_pr_dec_uint(&locked);
			ck_pr_dec_uint(&locked);
			ck_pr_dec_uint(&locked);
			ck_pr_dec_uint(&locked);
			ck_pr_dec_uint(&locked);

			l = ck_pr_load_uint(&locked);
			if (l != 0) {
				fprintf(stderr, "ERROR [WR:%d]: %u != 0\n", __LINE__, l);
				exit(EXIT_FAILURE);
			}
		}
		ck_bytelock_write_unlock(&lock);

		ck_bytelock_read_lock(&lock, context->tid);
		{
			l = ck_pr_load_uint(&locked);
			if (l != 0) {
				fprintf(stderr, "ERROR [RD:%d]: %u != 0\n", __LINE__, l);
				exit(EXIT_FAILURE);
			}
		}
		ck_bytelock_read_unlock(&lock, context->tid);
	}

	return (NULL);
}

int
main(int argc, char *argv[])
{
	pthread_t *threads;
	struct block *context;
	int i;

	if (argc != 3) {
		fprintf(stderr, "Usage: correct <number of threads> <affinity delta>\n");
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

	context = malloc(sizeof(struct block) * nthr);
	if (context == NULL) {
		fprintf(stderr, "ERROR: Could not allocate thread contexts\n");
		exit(EXIT_FAILURE);
	}

	a.delta = atoi(argv[2]);

	fprintf(stderr, "Creating threads (mutual exclusion)...");
	for (i = 0; i < nthr; i++) {
		context[i].tid = i + 1;
		if (pthread_create(&threads[i], NULL, thread, context + i)) {
			fprintf(stderr, "ERROR: Could not create thread %d\n", i);
			exit(EXIT_FAILURE);
		}
	}
	fprintf(stderr, "done\n");

	fprintf(stderr, "Waiting for threads to finish correctness regression...");
	for (i = 0; i < nthr; i++)
		pthread_join(threads[i], NULL);
	fprintf(stderr, "done (passed)\n");

	return (0);
}


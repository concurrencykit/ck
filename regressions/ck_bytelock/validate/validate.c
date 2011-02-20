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

#ifdef __linux__
#include <sched.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#endif

#ifndef CORES 
#define CORES 8
#endif

#ifndef ITERATE
#define ITERATE 128000 
#endif

struct affinity {
	uint32_t delta;
	uint32_t request;
};

struct block {
	unsigned int tid;
};

static struct affinity a;
static unsigned int locked = 0;
static int nthr;

static ck_bytelock_t lock = CK_BYTELOCK_INITIALIZER;

#ifdef __linux__
#ifndef gettid
static pid_t
gettid(void)
{
	return syscall(__NR_gettid);
}
#endif

static int
aff_iterate(struct affinity *acb)
{
	cpu_set_t s;
	int c;

	c = ck_pr_faa_32(&acb->request, acb->delta);
	CPU_ZERO(&s);
	CPU_SET(c % CORES, &s);

	return sched_setaffinity(gettid(), sizeof(s), &s);
}
#else
static int
aff_iterate(struct affinity *acb)
{
	acb = NULL;
	return (0);
}
#endif

static void *
thread(void *null)
{
	struct block *context = null;
	int i = 1000000;
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
	a.request = 0;

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


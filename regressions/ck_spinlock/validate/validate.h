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
#include <ck_spinlock.h>

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
#define ITERATE 1000000
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
static uint64_t nthr;

LOCK_DEFINE;

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
thread(void *null __attribute__((unused)))
{
#ifdef LOCK_STATE
	LOCK_STATE;
#endif
	unsigned int i = ITERATE;
	unsigned int j;

        if (aff_iterate(&a)) {
                perror("ERROR: Could not affine thread");
                exit(EXIT_FAILURE);
        }

	while (i--) {
		LOCK;

		ck_pr_inc_uint(&locked);
		ck_pr_inc_uint(&locked);
		ck_pr_inc_uint(&locked);
		ck_pr_inc_uint(&locked);
		ck_pr_inc_uint(&locked);

		j = ck_pr_load_uint(&locked);

		if (j != 5) {
			fprintf(stderr, "ERROR (WR): Race condition (%u)\n", j);
			exit(EXIT_FAILURE);
		}

		ck_pr_dec_uint(&locked);
		ck_pr_dec_uint(&locked);
		ck_pr_dec_uint(&locked);
		ck_pr_dec_uint(&locked);
		ck_pr_dec_uint(&locked);

		UNLOCK;
		LOCK;

		j = ck_pr_load_uint(&locked);
		if (j != 0) {
			fprintf(stderr, "ERROR (RD): Race condition (%u)\n", j);
			exit(EXIT_FAILURE);
		}
		UNLOCK;
	}

	return (NULL);
}

int
main(int argc, char *argv[])
{
	uint64_t i;
	pthread_t *threads;

	if (argc != 3) {
		fprintf(stderr, "Usage: " LOCK_NAME " <number of threads> <affinity delta>\n");
		exit(EXIT_FAILURE);
	}

	nthr = atoi(argv[1]);
	if (nthr <= 0) {
		fprintf(stderr, "ERROR: Number of threads must be greater than 0\n");
		exit(EXIT_FAILURE);
	}

#ifdef LOCK_INIT
	LOCK_INIT;
#endif

	threads = malloc(sizeof(pthread_t) * nthr);
	if (threads == NULL) {
		fprintf(stderr, "ERROR: Could not allocate thread structures\n");
		exit(EXIT_FAILURE);
	}

	a.delta = atoi(argv[2]);
	a.request = 0;

	fprintf(stderr, "Creating threads (mutual exclusion)...");
	for (i = 0; i < nthr; i++) {
		if (pthread_create(&threads[i], NULL, thread, NULL)) {
			fprintf(stderr, "ERROR: Could not create thread %" PRIu64 "\n", i);
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


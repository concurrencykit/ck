#include <ck_cc.h>
#include <ck_sequence.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__
#include <sched.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#endif

#ifndef CORES
#define CORES 8
#endif

#ifndef STEPS
#define STEPS 1000000
#endif

struct affinity {
	uint32_t delta;
	uint32_t request;
};

struct example {
        uint64_t a;
        uint64_t b;
        uint64_t c;
};

static struct example global CK_CC_CACHELINE;
static ck_sequence_t seqlock CK_CC_CACHELINE = CK_SEQUENCE_INITIALIZER;
static unsigned int barrier;
static struct affinity affinerator;

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
consumer(void *unused)
{
        struct example copy;
        uint32_t version;
        unsigned int counter = 0;
        unsigned int i;

	unused = NULL;
	if (aff_iterate(&affinerator)) {
		perror("ERROR: Could not affine thread");
		exit(EXIT_FAILURE);
	}

        while (ck_pr_load_uint(&barrier) == 0);
        for (i = 0; i < STEPS; i++) {
                /*
                 * Attempt a read of the data structure. If the structure
                 * has been modified between ck_sequence_read_begin and
                 * ck_sequence_read_retry then attempt another read since
                 * the data may be in an inconsistent state.
                 */
                do {
                        version = ck_sequence_read_begin(&seqlock);
                        copy = global;
                        counter++;
                } while (ck_sequence_read_retry(&seqlock, version));

		if (copy.b != copy.a + 1000) {
			fprintf(stderr, "ERROR: Failed regression: copy.b\n");
			exit(EXIT_FAILURE);
		}

		if (copy.c != copy.a + copy.b) {
			fprintf(stderr, "ERROR: Failed regression: copy.c\n");
			exit(EXIT_FAILURE);
		}
        }

        fprintf(stderr, "%u retries.\n", counter - STEPS);
	ck_pr_dec_uint(&barrier);
        return (NULL);
}

int
main(int argc, char *argv[])
{
	pthread_t *threads;
        unsigned int counter = 0;
	int n_threads, i;

	if (argc != 3) {
		fprintf(stderr, "Usage: ck_sequence <number of threads> <affinity delta>\n");
		exit(EXIT_FAILURE);
	}

	n_threads = atoi(argv[1]);
	if (n_threads <= 0) {
		fprintf(stderr, "ERROR: Number of threads must be greater than 0\n");
		exit(EXIT_FAILURE);
	}

	threads = malloc(sizeof(pthread_t) * n_threads);
	if (threads == NULL) {
		fprintf(stderr, "ERROR: Could not allocate memory for threads\n");
		exit(EXIT_FAILURE);
	}

	affinerator.delta = atoi(argv[2]);
	affinerator.request = 0;

	for (i = 0; i < n_threads; i++) {
		if (pthread_create(&threads[i], NULL, consumer, NULL)) {
			fprintf(stderr, "ERROR: Failed to create thread %d\n", i);
			exit(EXIT_FAILURE);
		}
	}

        for (;;) {
                /*
                 * Update the shared data in a non-blocking fashion.
		 * If the data is modified by multiple writers then
		 * ck_sequence_write_begin must be called after acquiring
		 * the associated lock and ck_sequence_write_end must be
		 * called before relinquishing the lock.
                 */
                ck_sequence_write_begin(&seqlock);
                global.a = counter++;
                ck_pr_store_64(&global.b, global.a + 1000);
                ck_pr_store_64(&global.c, global.b + global.a);
                ck_sequence_write_end(&seqlock);

		if (counter == 1)
			ck_pr_store_uint(&barrier, n_threads);

                counter++;
		if (ck_pr_load_uint(&barrier) == 0)
                        break;
        }

        printf("%u updates made.\n", counter);
        return (0);
}


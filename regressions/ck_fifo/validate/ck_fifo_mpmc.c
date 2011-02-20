#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <ck_fifo.h>

#ifdef CK_F_FIFO_MPMC
#ifdef __linux__
#include <sched.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#endif

#ifndef ITERATIONS
#define ITERATIONS 128 
#endif

#ifndef CORES
#define CORES 8
#endif

struct affinity {
        uint32_t delta;
        uint32_t request;
};

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

#ifdef CK_F_FIFO_MPMC
static ck_fifo_mpmc_t fifo;
#endif

static struct affinity a;
static int size;
static unsigned int barrier;

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
test(void *c)
{
#ifdef CK_F_FIFO_MPMC
	struct context *context = c;
	struct entry *entry;
	ck_fifo_mpmc_entry_t *fifo_entry;
	int i, j;

        if (aff_iterate(&a)) {
                perror("ERROR: Could not affine thread");
                exit(EXIT_FAILURE);
        }

	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) < (unsigned int)nthr);

	for (i = 0; i < ITERATIONS; i++) {
		for (j = 0; j < size; j++) {
			fifo_entry = malloc(sizeof(ck_fifo_mpmc_entry_t));
			entry = malloc(sizeof(struct entry));
			entry->tid = context->tid;
			ck_fifo_mpmc_enqueue(&fifo, fifo_entry, entry);
			if (ck_fifo_mpmc_dequeue(&fifo, &entry) == false) {
				fprintf(stderr, "ERROR [%u] Queue should never be empty.\n", context->tid);
				exit(EXIT_FAILURE);
			}

			if (entry->tid < 0 || entry->tid >= nthr) {
				fprintf(stderr, "ERROR [%u] Incorrect value in entry.\n", entry->tid);
				exit(EXIT_FAILURE);
			}
		}
	}
#endif

	return (NULL);
}

int
main(int argc, char *argv[])
{
	int i, r;
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
	assert(size > 0);

	context = malloc(sizeof(*context) * nthr);
	assert(context);

	thread = malloc(sizeof(pthread_t) * nthr);
	assert(thread);

	ck_fifo_mpmc_init(&fifo, malloc(sizeof(ck_fifo_mpmc_entry_t)));
	for (i = 0; i < nthr; i++) {
		context[i].tid = i;
		r = pthread_create(thread + i, NULL, test, context + i);
		assert(r == 0);
	}

	for (i = 0; i < nthr; i++)
		pthread_join(thread[i], NULL);

	return (0);
}
#else
int
main(void)
{
	fprintf(stderr, "Unsupported.\n");
	return 0;
}
#endif


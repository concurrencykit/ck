#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include <ck_hp_fifo.h>

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

static ck_hp_fifo_t fifo;
static ck_hp_t fifo_hp;
static int nthr;

static struct affinity a;
static int size;
static unsigned int barrier;
static unsigned int e_barrier;

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
	struct context *context = c;
	struct entry *entry;
	ck_hp_fifo_entry_t *fifo_entry;
	ck_hp_record_t record;
	int i, j;

        if (aff_iterate(&a)) {
                perror("ERROR: Could not affine thread");
                exit(EXIT_FAILURE);
        }

	ck_hp_subscribe(&fifo_hp, &record, malloc(sizeof(void *) * 2));
	ck_pr_inc_uint(&barrier);
	while (ck_pr_load_uint(&barrier) < (unsigned int)nthr);

	for (i = 0; i < ITERATIONS; i++) {
		for (j = 0; j < size; j++) {
			fifo_entry = malloc(sizeof(ck_hp_fifo_entry_t));
			entry = malloc(sizeof(struct entry));
			entry->tid = context->tid;
			ck_hp_fifo_enqueue_mpmc(&record, &fifo, fifo_entry, entry);

			fifo_entry = ck_hp_fifo_dequeue_mpmc(&record, &fifo, &entry);
			if (fifo_entry == NULL) {
				fprintf(stderr, "ERROR [%u] Queue should never be empty.\n", context->tid);
				exit(EXIT_FAILURE);
			}

			if (entry->tid < 0 || entry->tid >= nthr) {
				fprintf(stderr, "ERROR [%u] Incorrect value in entry.\n", entry->tid);
				exit(EXIT_FAILURE);
			}

			ck_hp_free(&record, &fifo_entry->hazard, fifo_entry, fifo_entry);
		}
	}

	ck_pr_inc_uint(&e_barrier);
	while (ck_pr_load_uint(&e_barrier) < (unsigned int)nthr);

	return (NULL);
}

static void
destructor(void *p)
{

	free(p);
	return;
}

int
main(int argc, char *argv[])
{
	int i, r;
	struct context *context;
	pthread_t *thread;
	int threshold;

	if (argc != 5) {
		fprintf(stderr, "Usage: validate <threads> <affinity delta> <size> <threshold>\n");
		exit(EXIT_FAILURE);
	}

	a.request = 0;
	a.delta = atoi(argv[2]);

	nthr = atoi(argv[1]);
	assert(nthr >= 1);

	size = atoi(argv[3]);
	assert(size > 0);

	threshold = atoi(argv[4]);
	assert(threshold > 0);

	context = malloc(sizeof(*context) * nthr);
	assert(context);

	thread = malloc(sizeof(pthread_t) * nthr);
	assert(thread);

	ck_hp_init(&fifo_hp, 2, threshold, destructor);
	ck_hp_fifo_init(&fifo, malloc(sizeof(ck_hp_fifo_entry_t)));
	for (i = 0; i < nthr; i++) {
		context[i].tid = i;
		r = pthread_create(thread + i, NULL, test, context + i);
		assert(r == 0);
	}

	for (i = 0; i < nthr; i++)
		pthread_join(thread[i], NULL);

	return (0);
}


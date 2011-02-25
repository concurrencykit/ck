#include <ck_hp.h>
#include <ck_hp_fifo.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../common.h"

#ifndef ENTRIES
#define ENTRIES 4096 
#endif

#ifndef STEPS
#define STEPS 40000
#endif

ck_hp_fifo_t fifo;
ck_hp_t fifo_hp;

int
main(void)
{
	void *r;
	uint64_t s, e, a;
	unsigned int i;
	unsigned int j;
	ck_hp_fifo_entry_t hp_entry[ENTRIES];
	ck_hp_fifo_entry_t hp_stub;
	ck_hp_record_t record;

	ck_hp_init(&fifo_hp, CK_HP_FIFO_SLOTS_COUNT, 1000000, NULL);

	r = malloc(CK_HP_FIFO_SLOTS_SIZE);
	if (r == NULL) {
		fprintf(stderr, "ERROR: Failed to allocate slots.\n");
		exit(EXIT_FAILURE);
	}
	ck_hp_subscribe(&fifo_hp, &record, r);

	a = 0;
	for (i = 0; i < STEPS; i++) {
		ck_hp_fifo_init(&fifo, &hp_stub);

		s = rdtsc();
		for (j = 0; j < ENTRIES; j++)
			ck_hp_fifo_enqueue_mpmc(&record, &fifo, hp_entry + j, NULL);
		e = rdtsc();

		a += e - s;
	}
	printf("ck_hp_fifo_enqueue_mpmc: %16" PRIu64 "\n", a / STEPS / ENTRIES);

	a = 0;
	for (i = 0; i < STEPS; i++) {
		ck_hp_fifo_init(&fifo, &hp_stub);
		for (j = 0; j < ENTRIES; j++)
			ck_hp_fifo_enqueue_mpmc(&record, &fifo, hp_entry + j, NULL);

		s = rdtsc();
		for (j = 0; j < ENTRIES; j++)
			ck_hp_fifo_dequeue_mpmc(&record, &fifo, &r);
		e = rdtsc();
		a += e - s;
	}
	printf("ck_hp_fifo_dequeue_mpmc: %16" PRIu64 "\n", a / STEPS / ENTRIES);

	return 0;
}

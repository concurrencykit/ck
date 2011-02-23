#include <ck_stack.h>
#include <ck_spinlock.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "../../common.h"

#ifndef ENTRIES
#define ENTRIES 1024
#endif

#ifndef STEPS
#define STEPS 4000
#endif

static ck_stack_t stack;

int
main(void)
{
	ck_stack_entry_t entry[4096];
	ck_spinlock_fas_t mutex = CK_SPINLOCK_FAS_INITIALIZER;
	volatile ck_stack_entry_t * volatile r;
	uint64_t s, e, a;
	unsigned int i;
	unsigned int j;

	a = 0;
	for (i = 0; i < STEPS; i++) {
		ck_stack_init(&stack);

		s = rdtsc();
		for (j = 0; j < sizeof(entry) / sizeof(*entry); j++) {
			ck_spinlock_fas_lock(&mutex);
			ck_stack_push_spnc(&stack, entry + j);
			ck_spinlock_fas_unlock(&mutex);
		}
		e = rdtsc();

		a += e - s;
	}
	printf("     push_spinlock: %16" PRIu64 "\n", a / STEPS / (sizeof(entry) / sizeof(*entry)));

#ifdef CK_F_STACK_PUSH_UPMC
	a = 0;
	for (i = 0; i < STEPS; i++) {
		ck_stack_init(&stack);

		s = rdtsc();
		for (j = 0; j < sizeof(entry) / sizeof(*entry); j++)
			ck_stack_push_upmc(&stack, entry + j);
		e = rdtsc();

		a += e - s;
	}
	printf("ck_stack_push_upmc: %16" PRIu64 "\n", a / STEPS / (sizeof(entry) / sizeof(*entry)));
#endif /* CK_F_STACK_PUSH_UPMC */

#ifdef CK_F_STACK_PUSH_MPMC
	a = 0;
	for (i = 0; i < STEPS; i++) {
		ck_stack_init(&stack);

		s = rdtsc();
		for (j = 0; j < sizeof(entry) / sizeof(*entry); j++)
			ck_stack_push_mpmc(&stack, entry + j);
		e = rdtsc();

		a += e - s;
	}
	printf("ck_stack_push_mpmc: %16" PRIu64 "\n", a / STEPS / (sizeof(entry) / sizeof(*entry)));
#endif /* CK_F_STACK_PUSH_MPMC */

#ifdef CK_F_STACK_PUSH_MPNC
	a = 0;
	for (i = 0; i < STEPS; i++) {
		ck_stack_init(&stack);

		s = rdtsc();
		for (j = 0; j < sizeof(entry) / sizeof(*entry); j++)
			ck_stack_push_mpnc(&stack, entry + j);
		e = rdtsc();

		a += e - s;
	}
	printf("ck_stack_push_mpnc: %16" PRIu64 "\n", a / STEPS / (sizeof(entry) / sizeof(*entry)));
#endif /* CK_F_STACK_PUSH_MPNC */

#if defined(CK_F_STACK_PUSH_UPMC) && defined(CK_F_STACK_POP_UPMC)
	a = 0;
	for (i = 0; i < STEPS; i++) {
		ck_stack_init(&stack);

		for (j = 0; j < sizeof(entry) / sizeof(*entry); j++)
			ck_stack_push_upmc(&stack, entry + j);

		s = rdtsc();
		for (j = 0; j < sizeof(entry) / sizeof(*entry); j++)
			r = ck_stack_pop_upmc(&stack);
		e = rdtsc();
		a += e - s;
	}
	printf(" ck_stack_pop_upmc: %16" PRIu64 "\n", a / STEPS / (sizeof(entry) / sizeof(*entry)));
#endif /* CK_F_STACK_PUSH_UPMC && CK_F_STACK_POP_UPMC */

#if defined(CK_F_STACK_POP_MPMC) && defined(CK_F_STACK_PUSH_MPMC)
	a = 0;
	for (i = 0; i < STEPS; i++) {
		ck_stack_init(&stack);

		for (j = 0; j < sizeof(entry) / sizeof(*entry); j++)
			ck_stack_push_mpmc(&stack, entry + j);

		s = rdtsc();
		for (j = 0; j < sizeof(entry) / sizeof(*entry); j++)
			r = ck_stack_pop_mpmc(&stack);
		e = rdtsc();
		a += e - s;
	}
	printf(" ck_stack_pop_mpmc: %16" PRIu64 "\n", a / STEPS / (sizeof(entry) / sizeof(*entry)));
#endif

	return 0;
}

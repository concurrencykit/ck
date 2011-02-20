#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <ck_stack.h>

#ifndef SIZE
#define SIZE 1024000
#endif

struct entry {
	int value;
	ck_stack_entry_t next;
};

CK_STACK_CONTAINER(struct entry, next, get_entry)

#define LOOP(PUSH, POP)								\
	for (i = 0; i < SIZE; i++) {						\
		entries[i].value = i;						\
		PUSH(stack, &entries[i].next);					\
	}									\
	for (i = SIZE - 1; i >= 0; i--) {					\
		entry = POP(stack);						\
		assert(entry);							\
		assert(get_entry(entry)->value == i);				\
	}

static void
serial(ck_stack_t *stack)
{
	struct entry *entries;
	ck_stack_entry_t *entry;
	int i;

	ck_stack_init(stack);

	entries = malloc(sizeof(struct entry) * SIZE);
	assert(entries != NULL);

	LOOP(ck_stack_push_upmc, ck_stack_pop_upmc);
#ifdef CK_F_STACK_POP_MPMC
	LOOP(ck_stack_push_mpmc, ck_stack_pop_mpmc);
#endif
	LOOP(ck_stack_push_mpnc, ck_stack_pop_upmc);
	LOOP(ck_stack_push_spnc, ck_stack_pop_npsc);

	return;
}

int
main(void)
{
	ck_stack_t stack;

	serial(&stack);
	return (0);
}

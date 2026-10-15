/*
 * Copyright 2009-2015 Samy Al Bahra.
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

/*
 * The batch pop implementations are exercised through volatile
 * function pointers in the cold single-shot tests below, where the
 * compiler would otherwise warn about failing to honor the inline
 * request.
 */
static ck_stack_entry_t *(*volatile batch_pop)(ck_stack_t *) =
    ck_stack_batch_pop_mpmc;
static ck_stack_entry_t *(*volatile batch_pop_wf)(ck_stack_t *) =
    ck_stack_batch_pop_mpmc_wf;

/*
 * Validate that a batch pop implementation advances the generation
 * counter on every platform, with or without a double-wide CAS.
 */
static void
batch_advance(ck_stack_t *stack, ck_stack_entry_t *(*batch)(ck_stack_t *))
{
	ck_stack_entry_t *entry;
	struct entry a;
	char *generation;

	ck_stack_init(stack);
	ck_stack_push_mpmc(stack, &a.next);

	generation = ck_pr_load_ptr(&stack->generation);
	entry = batch(stack);
	assert(entry == &a.next);
	assert(ck_pr_load_ptr(&stack->generation) == generation + 1);
	assert(CK_STACK_ISEMPTY(stack));
	return;
}

#ifdef CK_F_STACK_POP_MPMC
/*
 * Validate that a batch pop implementation advances the generation
 * counter: a ck_stack_pop_mpmc whose (head, generation) snapshot
 * predates the batch removal must not succeed against a re-pushed
 * head entry, otherwise it installs a successor owned by the batch
 * popper (ABA).
 */
static void
batch_generation(ck_stack_t *stack, ck_stack_entry_t *(*batch)(ck_stack_t *))
{
	struct ck_stack original, update;
	ck_stack_entry_t *entry;
	struct entry a, b;

	ck_stack_init(stack);
	ck_stack_push_mpmc(stack, &b.next);
	ck_stack_push_mpmc(stack, &a.next);

	/*
	 * Equivalent to a concurrent ck_stack_pop_mpmc that stalls after
	 * computing its target state.
	 */
	original.generation = ck_pr_load_ptr(&stack->generation);
	ck_pr_fence_load();
	original.head = ck_pr_load_ptr(&stack->head);
	update.generation = original.generation + 1;
	update.head = original.head->next;

	/* The batch removal must invalidate the snapshot above. */
	entry = batch(stack);
	assert(entry == &a.next);

	/* The previous head address is re-used by a producer. */
	ck_stack_push_mpmc(stack, &a.next);

	/* The stalled pop now resumes and must fail. */
	if (ck_pr_cas_ptr_2(stack, &original, &update) == true) {
		fprintf(stderr, "ERROR: Stale pop succeeded after batch pop "
		    "(generation was not advanced).\n");
		exit(EXIT_FAILURE);
	}

	/*
	 * The stack must be left with a as its only entry, with the
	 * generation advanced exactly once by the batch removal.
	 */
	assert(ck_pr_load_ptr(&stack->head) == &a.next);
	assert(ck_pr_load_ptr(&stack->generation) == original.generation + 1);
	assert(a.next.next == NULL);
	return;
}
#endif

int
main(void)
{
	ck_stack_t stack CK_CC_CACHELINE;

	serial(&stack);
	batch_advance(&stack, batch_pop);
	batch_advance(&stack, batch_pop_wf);
#ifdef CK_F_STACK_POP_MPMC
	batch_generation(&stack, batch_pop);
	batch_generation(&stack, batch_pop_wf);
#endif
	return (0);
}

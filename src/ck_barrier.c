/*
 * Copyright 2011 Samy Al Bahra.
 * Copyright 2011 David Joseph.
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

#include <ck_barrier.h>
#include <ck_cc.h>
#include <ck_pr.h>
#include <ck_spinlock.h>

#include <stdio.h>

struct ck_barrier_combining_queue {
	struct ck_barrier_combining_group *head;
	struct ck_barrier_combining_group *tail;
};

/*
 * Log and power_2 algorithms from: http://graphics.stanford.edu/~seander/bithacks.html
 */
CK_CC_INLINE static unsigned int
ck_barrier_internal_log(unsigned int v)
{
	static const unsigned int b[] = {0xAAAAAAAA, 0xCCCCCCCC, 0xF0F0F0F0, 
					 0xFF00FF00, 0xFFFF0000};

	register unsigned int r = (v & b[0]) != 0;
	int i;

	for (i = 4; i > 0; i--) {
		r |= ((v & b[i]) != 0) << i;
	}

	return (r);
}

CK_CC_INLINE static unsigned int
ck_barrier_internal_power_2(unsigned int v)
{

	--v;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	++v;

	return (v);
}

void
ck_barrier_centralized(struct ck_barrier_centralized *barrier,
		       struct ck_barrier_centralized_state *state,
		       unsigned int n_threads)
{
	unsigned int sense, value;

	sense = state->sense = ~state->sense;
	value = ck_pr_faa_uint(&barrier->value, 1);
	if (value == n_threads - 1) {
		ck_pr_store_uint(&barrier->value, 0);
		ck_pr_store_uint(&barrier->sense, sense);
		return;
	}

	while (sense != ck_pr_load_uint(&barrier->sense))
		ck_pr_stall();

	return;
}

CK_CC_INLINE static void
ck_barrier_combining_queue_enqueue(struct ck_barrier_combining_queue *queue,
				   struct ck_barrier_combining_group *node_value)
{

	node_value->next = NULL;

	if (queue->head == NULL) {
		queue->head = queue->tail = node_value;
		return;
	}

	queue->tail->next = node_value;
	queue->tail = node_value;

	return;
}

CK_CC_INLINE static struct ck_barrier_combining_group *
ck_barrier_combining_queue_dequeue(struct ck_barrier_combining_queue *queue)
{
	struct ck_barrier_combining_group *front = NULL;

	if (queue->head != NULL) {
		front = queue->head;
		queue->head = queue->head->next;
	}

	return (front);
}

CK_CC_INLINE static bool
ck_barrier_combining_try_insert(struct ck_barrier_combining_group *parent,
				struct ck_barrier_combining_group *tnode,
				struct ck_barrier_combining_group **child)
{

	if (*child == NULL) {
		*child = tnode;
		tnode->parent = parent;
		parent->k++;

		return (true);
	}

	return (false);
}

static void
ck_barrier_combining_aux(struct ck_barrier_combining *barrier,
			 struct ck_barrier_combining_group *tnode,
			 unsigned int sense)
{

	if (ck_pr_faa_uint(&tnode->count, 1) == tnode->k - 1) {
		if (tnode->parent != NULL)
			ck_barrier_combining_aux(barrier, tnode->parent, sense);

		ck_pr_store_uint(&tnode->count, 0);
		ck_pr_store_uint(&tnode->sense, ~tnode->sense);
	} else {
		while (sense != ck_pr_load_uint(&tnode->sense))
			ck_pr_stall();
	}

	return;
}

void
ck_barrier_combining_group_init(struct ck_barrier_combining *root,
				struct ck_barrier_combining_group *tnode,
				unsigned int nthr)
{
	struct ck_barrier_combining_group *node;
	struct ck_barrier_combining_queue queue;

	queue.head = queue.tail = NULL;

	tnode->k = nthr;
	tnode->count = 0;
	tnode->sense = 0;
	tnode->lchild = tnode->rchild = NULL;

	ck_spinlock_fas_lock(&root->mutex);
	ck_barrier_combining_queue_enqueue(&queue, root->root);
	while (queue.head != NULL) {
		node = ck_barrier_combining_queue_dequeue(&queue);

		if (ck_barrier_combining_try_insert(node, tnode, &node->lchild) == true)
			goto leave;

		if (ck_barrier_combining_try_insert(node, tnode, &node->rchild) == true)
			goto leave;

		ck_barrier_combining_queue_enqueue(&queue, node->lchild);
		ck_barrier_combining_queue_enqueue(&queue, node->rchild);
	}

leave:
	ck_spinlock_fas_unlock(&root->mutex);
	return;
}

void
ck_barrier_combining_init(struct ck_barrier_combining *root, 
			  struct ck_barrier_combining_group *init_root)
{

	init_root->k = 0;
	init_root->count = 0;
	init_root->sense = 0;
	init_root->parent = init_root->lchild = init_root->rchild = NULL;
	ck_spinlock_fas_init(&root->mutex);
	root->root = init_root;
	return;
}

void
ck_barrier_combining(struct ck_barrier_combining *barrier,
		     struct ck_barrier_combining_group *tnode,
		     struct ck_barrier_combining_state *state)
{

	ck_barrier_combining_aux(barrier, tnode, state->sense);
	state->sense = ~state->sense;
	return;
}

static unsigned int ck_barrier_dissemination_nthr;
static unsigned int ck_barrier_dissemination_tid;

void
ck_barrier_dissemination_init(struct ck_barrier_dissemination *barrier,
			      struct ck_barrier_dissemination_internal **barrier_internal,
			      unsigned int nthr)
{
	unsigned int i, j, k, size, offset;

	ck_barrier_dissemination_nthr = nthr;
	size = (ck_barrier_internal_log(ck_barrier_internal_power_2(nthr)));

	for (i = 0; i < nthr; ++i) {
		barrier[i].flags[0] = barrier_internal[i];
		barrier[i].flags[1] = barrier_internal[i] + size;
	}

	for (i = 0; i < nthr; ++i) {
		for (k = 0, offset = 1; k < size; ++k, offset <<= 1) {
			/* Determine the thread's partner, j, for the current round. */
			if ((nthr & (nthr - 1)) == 0)
				j = (i + offset) & (nthr - 1);
			else
				j = (i + offset) % nthr;

			/* Set the thread's partner for round k. */
			barrier[i].flags[0][k].pflag = &barrier[j].flags[0][k].tflag;
			barrier[i].flags[1][k].pflag = &barrier[j].flags[1][k].tflag;

			/* Set the thread's flags to false. */
			barrier[i].flags[0][k].tflag = barrier[i].flags[1][k].tflag = 0;
		}
	}

	return;
}

void
ck_barrier_dissemination_state_init(struct ck_barrier_dissemination_state *state)
{

	state->parity = 0;
	state->sense = ~0;
	state->tid = ck_pr_faa_uint(&ck_barrier_dissemination_tid, 1);
	return;
}

unsigned int
ck_barrier_dissemination_size(unsigned int nthr)
{

	return (ck_barrier_internal_log(ck_barrier_internal_power_2(nthr)) * 2);
}

void
ck_barrier_dissemination(struct ck_barrier_dissemination *barrier,
			 struct ck_barrier_dissemination_state *state)
{
	unsigned int i, size;

	size = (ck_barrier_internal_log(ck_barrier_internal_power_2(ck_barrier_dissemination_nthr)));
	for (i = 0; i < size; ++i) {
		/* Unblock current partner. */
		ck_pr_store_uint(barrier[state->tid].flags[state->parity][i].pflag, state->sense);

		/* Wait until some other thread unblocks this one. */
		while (ck_pr_load_uint(&barrier[state->tid].flags[state->parity][i].tflag) != state->sense)
			ck_pr_stall();
	}

	/*
	 * Dissemination barriers use two sets of flags to prevent race conditions
	 * between successive calls to the barrier. It also uses
	 * a sense reversal technique to avoid re-initialization of the flags
	 * for every two calls to the barrier.
	 */
	if (state->parity == 1)
		state->sense = ~state->sense;
	state->parity = 1 - state->parity;

	return;
}

static unsigned int ck_barrier_tournament_tid;

void
ck_barrier_tournament_state_init(ck_barrier_tournament_state_t *state)
{

	state->sense = ~0;
	state->vpid = ck_pr_faa_uint(&ck_barrier_tournament_tid, 1);
	return;
}

void
ck_barrier_tournament_round_init(struct ck_barrier_tournament_round **rounds,
                                 unsigned int nthr)
{
	unsigned int i, k, size, twok, twokm1, imod2k;

	size = ck_barrier_tournament_size(nthr);
	for (i = 0; i < nthr; ++i) {
		/*
		 * By intializing this outside of the inner loop, we can avoid
		 * checking k > 0 for every iteration.
		 */
		rounds[i][0].flag = 0;
		rounds[i][0].role = DROPOUT;
		for (k = 1, twok = 2, twokm1 = 1; k < size; ++k, twokm1 = twok, twok <<= 1) {
			rounds[i][k].flag = 0;

			imod2k = i & (twok - 1);
			if (imod2k == 0) {
				if ((i + twokm1 < nthr) && (twok < nthr))
					rounds[i][k].role = WINNER;
				else if (i + twokm1 >= nthr)
					rounds[i][k].role = BYE;
			}
			if (imod2k == twokm1)
				rounds[i][k].role = LOSER;
			else if ((i == 0) && (twok >= nthr))
				rounds[i][k].role = CHAMPION;

			if (rounds[i][k].role == LOSER)
				rounds[i][k].opponent = &rounds[i - twokm1][k].flag;
			else if (rounds[i][k].role == WINNER || rounds[i][k].role == CHAMPION)
				rounds[i][k].opponent = &rounds[i + twokm1][k].flag;  
		}
	}

	return;
}

unsigned int
ck_barrier_tournament_size(unsigned int nthr)
{

	return (ck_barrier_internal_log(ck_barrier_internal_power_2(nthr)) + 1);
}

void
ck_barrier_tournament(struct ck_barrier_tournament_round **rounds,
                      struct ck_barrier_tournament_state *state)
{
	int round = 1;

	for (;; ++round) {
		switch (rounds[state->vpid][round].role) { // MIGHT NEED TO USE CK_PR_LOAD
		case BYE:
			break;
		case CHAMPION:
			while (ck_pr_load_uint(&rounds[state->vpid][round].flag) != state->sense)
				ck_pr_stall();
			ck_pr_store_uint(rounds[state->vpid][round].opponent, state->sense);
			goto wakeup;
			break;
		case DROPOUT:
			/* NOTREACHED */
			break;
		case LOSER:
			ck_pr_store_uint(rounds[state->vpid][round].opponent, state->sense);
			while (ck_pr_load_uint(&rounds[state->vpid][round].flag) != state->sense)
				ck_pr_stall();
			goto wakeup;
			break;
		case WINNER:
			while (ck_pr_load_uint(&rounds[state->vpid][round].flag) != state->sense)
				ck_pr_stall();
			break;
		}
	}
wakeup:
	for (round -= 1;; --round) {
		switch (rounds[state->vpid][round].role) { // MIGHT NEED TO USE CK_PR_LOAD
		case BYE:
			break;
		case CHAMPION:
			/* NOTREACHED */
			break;
		case DROPOUT:
			goto leave;
			break;
		case LOSER:
			/* NOTREACHED */
			break;
		case WINNER:
			ck_pr_store_uint(rounds[state->vpid][round].opponent, state->sense);
			break;
		}
	}

leave:
	state->sense = ~state->sense;
	return;
}

static unsigned int ck_barrier_mcs_tid;

void
ck_barrier_mcs_init(struct ck_barrier_mcs *barrier,
                    unsigned int nthr)
{
	unsigned int i, j;

	for (i = 0; i < nthr; ++i) {
		for (j = 0; j < 4; ++j) {
			barrier[i].havechild[j] = ((i << 2) + j < nthr - 1) 	?
						  ~0 		     	 	:
						   0;
			barrier[i].childnotready[j] = barrier[i].havechild[j];
		}

		barrier[i].parent = (i == 0) 						?
				    &barrier[i].dummy					:
				    &barrier[(i - 1) >> 2].childnotready[(i - 1) & 3];

		barrier[i].children[0] = ((i << 1) + 1 >= nthr)			?
					 &barrier[i].dummy			:
					 &barrier[(i << 1) + 1].parentsense;

		barrier[i].children[1] = ((i << 1) + 2 >= nthr)			?
					 &barrier[i].dummy			:
					 &barrier[(i << 1) + 2].parentsense;

		barrier[i].parentsense = 0;
	}

	return;
}

void
ck_barrier_mcs_state_init(struct ck_barrier_mcs_state *state)
{

	state->sense = ~0;
	state->vpid = ck_pr_faa_uint(&ck_barrier_mcs_tid, 1);
	return;
}

CK_CC_INLINE static bool
ck_barrier_mcs_check_children(unsigned int *childnotready)
{
	int i;

	for (i = 0; i < 4; ++i) {
		if (ck_pr_load_uint(&childnotready[i]) != 0)
			return (false);
	}

	return (true);
}

CK_CC_INLINE static void
ck_barrier_mcs_reinitialize_children(struct ck_barrier_mcs *node)
{
	int i;

	for (i = 0; i < 4; ++i)
		ck_pr_store_uint(&node->childnotready[i], node->havechild[i]);

	return;
}

void
ck_barrier_mcs(struct ck_barrier_mcs *barrier,
               struct ck_barrier_mcs_state *state)
{

	while (ck_barrier_mcs_check_children(barrier[state->vpid].childnotready) == false)
		ck_pr_stall();
	ck_barrier_mcs_reinitialize_children(&barrier[state->vpid]);

	ck_pr_store_uint(barrier[state->vpid].parent, 0);

	if (state->vpid != 0) {
		while (ck_pr_load_uint(&barrier[state->vpid].parentsense) != state->sense)
			ck_pr_stall();
	}

	ck_pr_store_uint(barrier[state->vpid].children[0], state->sense);
	ck_pr_store_uint(barrier[state->vpid].children[1], state->sense);
	state->sense = ~state->sense;

	return;
}


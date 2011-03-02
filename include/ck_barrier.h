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

#ifndef _CK_BARRIER_H
#define _CK_BARRIER_H

#include <ck_cc.h>
#include <ck_pr.h>
#include <ck_spinlock.h>

#ifndef CK_F_BARRIER_CENTRALIZED
#define CK_F_BARRIER_CENTRALIZED

struct ck_barrier_centralized {
	unsigned int value;
	unsigned int sense;
};
typedef struct ck_barrier_centralized ck_barrier_centralized_t;

struct ck_barrier_centralized_state {
	unsigned int sense;
};
typedef struct ck_barrier_centralized_state ck_barrier_centralized_state_t;

#define CK_BARRIER_CENTRALIZED_INITIALIZER 	 {0, 0}
#define CK_BARRIER_CENTRALIZED_STATE_INITIALIZER {0}

CK_CC_INLINE static void
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

#endif /* CK_F_BARRIER_CENTRALIZED */

#ifndef CK_F_BARRIER_COMBINING
#define CK_F_BARRIER_COMBINING

struct ck_barrier_combining_group {
	unsigned int k;
	unsigned int count;
	unsigned int sense;
	struct ck_barrier_combining_group *parent;
	struct ck_barrier_combining_group *lchild;
	struct ck_barrier_combining_group *rchild;
	struct ck_barrier_combining_group *next;
};

typedef struct ck_barrier_combining_group ck_barrier_combining_group_t;

struct ck_barrier_combining_state {
	unsigned int sense;
};

typedef struct ck_barrier_combining_state ck_barrier_combining_state_t;

#define CK_BARRIER_COMBINING_STATE_INITIALIZER {~0}

struct ck_barrier_combining {
	struct ck_barrier_combining_group *root;
	ck_spinlock_fas_t mutex;
};

typedef struct ck_barrier_combining ck_barrier_combining_t;

struct ck_barrier_combining_queue {
	struct ck_barrier_combining_group *head;
	struct ck_barrier_combining_group *tail;
};

CK_CC_INLINE static void
ck_barrier_combining_queue_init(struct ck_barrier_combining_queue *queue)
{
	queue->head = queue->tail = NULL;
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

CK_CC_INLINE static void
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

CK_CC_INLINE static void
ck_barrier_combining_group_init(struct ck_barrier_combining *root,
				struct ck_barrier_combining_group *tnode,
				unsigned int nthr)
{
	struct ck_barrier_combining_group *node;
	struct ck_barrier_combining_queue queue;

	ck_barrier_combining_queue_init(&queue);

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

CK_CC_INLINE static void
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

CK_CC_INLINE static void
ck_barrier_combining(struct ck_barrier_combining *barrier,
		     struct ck_barrier_combining_group *tnode,
		     struct ck_barrier_combining_state *state)
{
	ck_barrier_combining_aux(barrier, tnode, state->sense);
	state->sense = ~state->sense;

	return;
}
#endif /* CK_F_BARRIER_COMBINING */

#endif /* _CK_BARRIER_H */


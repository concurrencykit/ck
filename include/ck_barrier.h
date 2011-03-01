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

#ifndef CK_BARRIER_CENTRALIZED
#define CK_BARRIER_CENTRALIZED

struct ck_barrier_centralized {
	unsigned int value;
	unsigned int sense;
};
typedef struct ck_barrier_centralized ck_barrier_centralized_t;

struct ck_barrier_centralized_state {
	unsigned int sense;
};
typedef struct ck_barrier_centralized_state ck_barrier_centralized_state_t;

#define CK_BARRIER_CENTRALIZED_INITIALIZER {0, 0}
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

#endif /* CK_BARRIER_CENTRALIZED */

#ifndef CK_BARRIER_COMBINING
#define CK_BARRIER_COMBINING

struct ck_barrier_combining_entry {
	unsigned int k;
	unsigned int count;
	unsigned int sense;
	struct ck_barrier_combining_entry *parent;
	struct ck_barrier_combining_entry *lchild;
	struct ck_barrier_combining_entry *rchild;
	struct ck_barrier_combining_entry *next;
};

typedef struct ck_barrier_combining_entry ck_barrier_combining_entry_t;

struct ck_barrier_combining_state {
	unsigned int sense;
};

typedef struct ck_barrier_combining_state ck_barrier_combining_state_t;

#define CK_BARRIER_COMBINING_STATE_INITIALIZER {~0}

struct ck_barrier_combining {
	struct ck_barrier_combining_entry *root;
	ck_spinlock_cas_t mutex;
};

typedef struct ck_barrier_combining ck_barrier_combining_t;

struct ck_barrier_ct_queue {
	struct ck_barrier_combining_entry *head;
	struct ck_barrier_combining_entry *tail;
};

CK_CC_INLINE static void
ck_barrier_ct_queue_init(struct ck_barrier_ct_queue *queue)
{
	queue->head = queue->tail = NULL;
	return;
}

CK_CC_INLINE static void
ck_barrier_ct_queue_enqueue(struct ck_barrier_ct_queue *queue,
			    struct ck_barrier_combining_entry *node_value)
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

CK_CC_INLINE static struct ck_barrier_combining_entry *
ck_barrier_ct_queue_dequeue(struct ck_barrier_ct_queue *queue)
{
	struct ck_barrier_combining_entry *front = NULL;

	if (queue->head != NULL) {
		front = queue->head;
		queue->head = queue->head->next;
	}

	return (front);
}

CK_CC_INLINE static bool
ck_barrier_ct_queue_is_empty(struct ck_barrier_ct_queue *queue)
{
	return (queue->head == NULL);
}

CK_CC_INLINE static void
ck_barrier_combining_init(struct ck_barrier_combining *root, 
			  struct ck_barrier_combining_entry *init_root)
{
	init_root->k = 0;
	init_root->count = 0;
	init_root->sense = 0;
	init_root->parent = init_root->lchild = init_root->rchild = NULL;

	ck_spinlock_cas_init(&root->mutex);
	root->root = init_root;

	return;
}

CK_CC_INLINE static bool
ck_barrier_combining_try_insert(struct ck_barrier_combining_entry *parent,
				struct ck_barrier_combining_entry *tnode,
				struct ck_barrier_combining_entry **child)
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
ck_barrier_combining_entry_init(struct ck_barrier_combining *root,
				struct ck_barrier_combining_entry *tnode)
{
	struct ck_barrier_combining_entry *node;
	struct ck_barrier_ct_queue queue;

	ck_barrier_ct_queue_init(&queue);

	tnode->k = 1;
	tnode->count = 0;
	tnode->sense = 0;
	tnode->lchild = tnode->rchild = NULL;

	ck_spinlock_cas_lock(&root->mutex);

	ck_barrier_ct_queue_enqueue(&queue, root->root);
	while (!ck_barrier_ct_queue_is_empty(&queue)) {
		node = ck_barrier_ct_queue_dequeue(&queue);
		if (ck_barrier_combining_try_insert(node, tnode, &node->lchild) == true) {
			ck_spinlock_cas_unlock(&root->mutex);
			return;
		}
		if (ck_barrier_combining_try_insert(node, tnode, &node->rchild) == true) {
			ck_spinlock_cas_unlock(&root->mutex);
			return;
		}
		ck_barrier_ct_queue_enqueue(&queue, node->lchild);
		ck_barrier_ct_queue_enqueue(&queue, node->rchild);
	}
}

CK_CC_INLINE static void
ck_barrier_combining_aux(struct ck_barrier_combining *barrier,
			 struct ck_barrier_combining_entry *tnode,
			 unsigned int sense)
{
	/* Incrementing a leaf's count is unnecessary. */
	if (tnode->lchild == NULL) {
		ck_barrier_combining_aux(barrier, tnode->parent, sense);
		ck_pr_store_uint(&tnode->sense, ~tnode->sense);
		return;
	}

	if (ck_pr_faa_uint(&tnode->count, 1) == tnode->k - 1) {
		if (tnode->parent != NULL)
			ck_barrier_combining_aux(barrier, tnode->parent, sense);
		ck_pr_store_uint(&tnode->count, 0);
		ck_pr_store_uint(&tnode->sense, ~tnode->sense);
	} else
		while (sense != ck_pr_load_uint(&tnode->sense)) {
			ck_pr_stall();
		}

	return;
}

CK_CC_INLINE static void
ck_barrier_combining(struct ck_barrier_combining *barrier,
		     struct ck_barrier_combining_entry *tnode,
		     struct ck_barrier_combining_state *state)
{
	ck_barrier_combining_aux(barrier, tnode, state->sense);
	state->sense = ~state->sense;
}

#endif /* CK_BARRIER_COMBINING */

#endif /* _CK_BARRIER_H */


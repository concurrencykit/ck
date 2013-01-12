/*
 * Copyright 2012-2013 Abel P. Mathew
 * Copyright 2012-2013 Samy Al Bahra
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

#ifndef _CK_BAG_H
#define _CK_BAG_H

#include <ck_cc.h>
#include <ck_malloc.h>
#include <ck_md.h>
#include <ck_pr.h>
#include <ck_queue.h>
#include <ck_stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * ck_bag is a lock-free spmc linked list of blocks.
 *
 * A block consists of:
 * 	next:
 * 		Linkage for bag linked list.
 * 	avail_next, avail_prev:
 * 		Linkage for bag's available linked list (to support 0(1) inserts).
 * 	array:
 * 		flexible array member.
 *
 * The top 3 bytes of "next" contain the # of entries within block->array.
 *
 * Valid entries in block->array are contigious and stored at the front
 * of the array. Empty entries are stored at the back.
 */

/*
 * Bag growth strategies.
 */
enum ck_bag_allocation_strategy {
	CK_BAG_ALLOCATE_GEOMETRIC = 0,
	CK_BAG_ALLOCATE_LINEAR
};

/*
 *	max: max n_entries per block
 *	bytes: sizeof(ck_bag_block) + sizeof(flex. array member)
 * 		+ inline allocator overhead
 */
struct ck_bag_block_info {
	size_t max;
	size_t bytes;
};

/*
 * Determine whether pointer packing should be enabled.
 */
#if defined(CK_MD_POINTER_PACK_ENABLE) && defined(CK_MD_VMA_BITS)
#define CK_BAG_PP
#endif

#define CK_BAG_DEFAULT 0

struct ck_bag_block_md {
#ifdef CK_BAG_PP
	struct ck_bag_block *ptr;
#else
	struct ck_bag_block *ptr;
	uintptr_t n_entries CK_CC_PACKED;
#endif
};

struct ck_bag_block {
	struct ck_bag_block_md next;
	CK_LIST_ENTRY(ck_bag_block) avail_entry;
	void *array[];
} CK_CC_CACHELINE;

struct ck_bag {
	struct ck_bag_block *head;
	CK_LIST_HEAD(avail_list, ck_bag_block) avail_blocks;
	unsigned int n_entries;
	unsigned int n_blocks;
	enum ck_bag_allocation_strategy alloc_strat;
	struct ck_bag_block_info info;
};
typedef struct ck_bag ck_bag_t;

struct ck_bag_iterator {
	struct ck_bag_block *block;
	uint16_t index;
	uint16_t n_entries;
};
typedef struct ck_bag_iterator ck_bag_iterator_t;

#ifdef CK_BAG_PP
#define CK_BAG_BLOCK_ENTRIES_MASK (~(uintptr_t)0 << CK_MD_VMA_BITS)
#endif

CK_CC_INLINE static struct ck_bag_block *
ck_bag_block_next(struct ck_bag_block *block)
{

#ifdef CK_BAG_PP
	return (struct ck_bag_block *)((uintptr_t)block & ~CK_BAG_BLOCK_ENTRIES_MASK);
#else
	return block;
#endif
}

CK_CC_INLINE static unsigned int
ck_bag_count(struct ck_bag *bag)
{

	return ck_pr_load_uint(&bag->n_entries);
}

CK_CC_INLINE static uint16_t
ck_bag_block_count(struct ck_bag_block *block)
{

#ifdef CK_BAG_PP
	return (uintptr_t)ck_pr_load_ptr(&block->next.ptr) >> CK_MD_VMA_BITS;
#else
	return (uintptr_t)ck_pr_load_ptr(&block->next.n_entries);
#endif
}

CK_CC_INLINE static void
ck_bag_iterator_init(ck_bag_iterator_t *iterator, ck_bag_t *bag)
{

	iterator->block = ck_pr_load_ptr(&bag->head);
	iterator->index = 0;
	iterator->n_entries = 0;
	if (iterator->block != NULL)
		iterator->n_entries = ck_bag_block_count(iterator->block);

	return;
}

CK_CC_INLINE static bool
ck_bag_next(struct ck_bag_iterator *iterator, void **entry)
{
	struct ck_bag_block *next;

	if (iterator->block == NULL)
		return NULL;

	if (iterator->index >= iterator->n_entries) {
		next = ck_pr_load_ptr(&iterator->block->next.ptr);
		iterator->block = ck_bag_block_next(next);
		if (iterator->block == NULL)
			return false;

		iterator->n_entries = ck_bag_block_count(iterator->block);
		if (iterator->n_entries == 0)
			return false;

		iterator->index = 0;
		ck_pr_fence_load();
	}

	*entry = ck_pr_load_ptr(&iterator->block->array[iterator->index++]);
	return true;
}

bool ck_bag_init(struct ck_bag *, size_t, enum ck_bag_allocation_strategy);
bool ck_bag_allocator_set(struct ck_malloc *, size_t);
void ck_bag_destroy(ck_bag_t *);
bool ck_bag_put_spmc(ck_bag_t *, void *);
bool ck_bag_set_spmc(struct ck_bag *, void *, void *);
bool ck_bag_remove_spmc(ck_bag_t *, void *);
bool ck_bag_member_spmc(ck_bag_t *, void *);

#endif /* _CK_BAG_H */

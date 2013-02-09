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

#include <ck_bag.h>
#include <ck_cc.h>
#include <ck_md.h>
#include <ck_pr.h>
#include <ck_malloc.h>
#include <ck_stdint.h>
#include <stdbool.h>
#include <string.h>

#define CK_BAG_PAGESIZE CK_MD_PAGESIZE

#ifdef CK_BAG_PP
#define CK_BAG_MAX_N_ENTRIES (1 << ((sizeof(void *) * 8) - CK_MD_VMA_BITS))
#endif

static struct ck_malloc allocator;
static size_t allocator_overhead;

bool
ck_bag_init(struct ck_bag *bag,
    size_t n_cachelines,
    enum ck_bag_allocation_strategy as)
{
	size_t block_overhead, block_size;

	CK_LIST_INIT(&bag->avail_blocks);
	bag->head = NULL;
	bag->n_entries = 0;
	bag->n_blocks = 0;
	bag->alloc_strat = as;

	block_overhead = sizeof(struct ck_bag_block) + allocator_overhead;

	block_size = (n_cachelines == CK_BAG_DEFAULT) ?
		CK_BAG_PAGESIZE : n_cachelines * CK_MD_CACHELINE;

	if (block_size < block_overhead)
		return false;

	bag->info.max = (block_size / sizeof(void *));

#ifdef CK_BAG_PP
	if (bag->info.max > CK_BAG_MAX_N_ENTRIES)
		return false;
#endif

	bag->info.bytes = block_overhead + sizeof(void *) * bag->info.max;
	return true;
}

void
ck_bag_destroy(struct ck_bag *bag)
{
	struct ck_bag_block *cursor = NULL;

	/*
	 * Free unoccupied blocks on the available list that are not linked to the
	 * bag list.
	 */
	CK_LIST_FOREACH(cursor, &bag->avail_blocks, avail_entry) {
		if (ck_bag_block_count(cursor) == 0) {
			CK_LIST_REMOVE(cursor, avail_entry);
			allocator.free(cursor, bag->info.bytes, false);
		}
	}

	cursor = bag->head;
	while (bag->head != NULL) {
		cursor = bag->head;
		bag->head = ck_bag_block_next(cursor->next.ptr);
		allocator.free(cursor, bag->info.bytes, false);
	}

	return;
}

bool
ck_bag_allocator_set(struct ck_malloc *m, size_t alloc_overhead)
{

	if (m->malloc == NULL || m->free == NULL)
		return false;

	allocator = *m;
	allocator_overhead = alloc_overhead;
	return true;
}

bool
ck_bag_put_spmc(struct ck_bag *bag, void *entry)
{
	struct ck_bag_block *cursor, *new_block, *new_bag_head, *prev_block;
	uint16_t n_entries_block;
	size_t blocks_alloc, i;
	uintptr_t next = 0;

	new_block = new_bag_head = prev_block = NULL;

	cursor = CK_LIST_FIRST(&bag->avail_blocks);
	if (cursor != NULL) {
		n_entries_block = ck_bag_block_count(cursor);
	} else {
		/* The bag is full, allocate a new set of blocks */
		prev_block = CK_LIST_FIRST(&bag->avail_blocks);
		if (bag->alloc_strat == CK_BAG_ALLOCATE_GEOMETRIC)
			blocks_alloc = (bag->n_blocks + 1) << 1;
		else
			blocks_alloc = 1;

		for (i = 0; i < blocks_alloc; i++) {
			new_block = allocator.malloc(bag->info.bytes);

			if (new_block == NULL)
				return false;

#ifdef CK_BAG_PP
			new_block->next.ptr = NULL;
#else
			new_block->next.n_entries = 0;
#endif
			if (i == 0) {
				new_bag_head = new_block;
				CK_LIST_INSERT_HEAD(&bag->avail_blocks, new_block, avail_entry);
			} else {
				CK_LIST_INSERT_AFTER(prev_block, new_block, avail_entry);
			}

			prev_block = new_block;
		}

		cursor = new_bag_head;
		n_entries_block = 0;
		bag->n_blocks += blocks_alloc;
	}

	cursor->array[n_entries_block++] = entry;
	ck_pr_fence_store();

#ifdef CK_BAG_PP
	next = ((uintptr_t)n_entries_block << CK_MD_VMA_BITS);
#endif


	if (n_entries_block == 1) {
		/* Place newly filled block at the head of bag list */
		if (bag->head != NULL) {
#ifdef CK_BAG_PP
			next += ((uintptr_t)(void *)ck_bag_block_next(bag->head));
#else
			next = (uintptr_t)(void *)ck_bag_block_next(bag->head);
#endif
		}

#ifndef CK_BAG_PP
		ck_pr_store_ptr(&cursor->next.n_entries, (void *)(uintptr_t)n_entries_block);
#endif

		ck_pr_store_ptr(&cursor->next.ptr, (void *)next);
		ck_pr_store_ptr(&bag->head, cursor);
	} else {
		/* Block is already on bag list, update n_entries */
#ifdef CK_BAG_PP
		next += ((uintptr_t)(void *)ck_bag_block_next(cursor->next.ptr));
		ck_pr_store_ptr(&cursor->next, (void *)next);
#else
		ck_pr_store_ptr(&cursor->next.n_entries, (void *)(uintptr_t)n_entries_block);
#endif
		/* the block is full, remove from available_list */
		if (n_entries_block == bag->info.max) {
			CK_LIST_REMOVE(cursor, avail_entry);
		}

	}

	ck_pr_store_uint(&bag->n_entries, bag->n_entries + 1);
	return true;
}

/*
 * Replace prev_entry with new entry if exists, otherwise insert into bag.
 */
bool
ck_bag_set_spmc(struct ck_bag *bag, void *compare, void *update)
{
	struct ck_bag_block *cursor;
	uint16_t block_index;
	uint16_t n_entries_block = 0;

	cursor = bag->head;
	while (cursor != NULL) {
		n_entries_block = ck_bag_block_count(cursor);
		for (block_index = 0; block_index < n_entries_block; block_index++) {
			if (cursor->array[block_index] != compare)
				continue;

			ck_pr_store_ptr(&cursor->array[block_index], update);
			return true;
		}

		cursor = ck_bag_block_next(cursor->next.ptr);
	}

	return ck_bag_put_spmc(bag, update);
}

bool
ck_bag_remove_spmc(struct ck_bag *bag, void *entry)
{
	struct ck_bag_block *cursor, *copy, *prev;
	uint16_t block_index, n_entries;

	cursor = bag->head;
	prev = NULL;
	while (cursor != NULL) {
		n_entries = ck_bag_block_count(cursor);

		for (block_index = 0; block_index < n_entries; block_index++) {
			if (cursor->array[block_index] == entry)
				goto found;

		}

		prev = cursor;
		cursor = ck_bag_block_next(cursor->next.ptr);
	}

	return true;

found:
	/* Cursor points to containing block, block_index is index of deletion */
	if (n_entries == 1) {
		if (prev == NULL) {
			struct ck_bag_block *new_head = ck_bag_block_next(cursor->next.ptr);
			ck_pr_store_ptr(&bag->head, new_head);
		} else {
			uintptr_t next;
#ifdef CK_BAG_PP
			next = ((uintptr_t)prev->next.ptr & (CK_BAG_BLOCK_ENTRIES_MASK)) |
				(uintptr_t)(void *)ck_bag_block_next(cursor->next.ptr);
#else
			next = (uintptr_t)(void *)cursor->next.ptr;
#endif
			ck_pr_store_ptr(&prev->next.ptr, (struct ck_bag_block *)next);
		}

		CK_LIST_REMOVE(cursor, avail_entry);
		bag->n_blocks--;
	} else {
		uintptr_t next_ptr;

		copy = allocator.malloc(bag->info.bytes);
		if (copy == NULL)
			return false;

		memcpy(copy, cursor, bag->info.bytes);
		copy->array[block_index] = copy->array[--n_entries];

		next_ptr = (uintptr_t)(void *)ck_bag_block_next(copy->next.ptr);
#ifdef CK_BAG_PP
		copy->next.ptr = (void *)(((uintptr_t)n_entries << CK_MD_VMA_BITS) | next_ptr);
#else
		copy->next.n_entries = n_entries;
		copy->next.ptr = (struct ck_bag_block *)next_ptr;
#endif

		ck_pr_fence_store();

		if (prev == NULL) {
			ck_pr_store_ptr(&bag->head, copy);
		} else {
#ifdef CK_BAG_PP
			uintptr_t next = ((uintptr_t)prev->next.ptr & (CK_BAG_BLOCK_ENTRIES_MASK)) |
				(uintptr_t)(void *)ck_bag_block_next(copy);
			ck_pr_store_ptr(&prev->next.ptr, (struct ck_bag_block *)next);
#else
			ck_pr_store_ptr(&prev->next.ptr, copy);
#endif
		}

		if (n_entries != bag->info.max-1) {
			/* Only remove cursor if it was previously on the avail_list */
			CK_LIST_REMOVE(cursor, avail_entry);
		}
		CK_LIST_INSERT_HEAD(&bag->avail_blocks, copy, avail_entry);
	}

	allocator.free(cursor, sizeof(struct ck_bag_block), true);
	ck_pr_store_uint(&bag->n_entries, bag->n_entries - 1);
	return true;
}

bool
ck_bag_member_spmc(struct ck_bag *bag, void *entry)
{
	struct ck_bag_block *cursor;
	uint16_t block_index, n_entries;

	if (bag->head == NULL)
		return NULL;

	cursor = ck_pr_load_ptr(&bag->head);
	while (cursor != NULL) {
		n_entries = ck_bag_block_count(cursor);
		for (block_index = 0; block_index < n_entries; block_index++) {
			if (ck_pr_load_ptr(&cursor->array[block_index]) == entry)
				return true;
		}
		cursor = ck_bag_block_next(ck_pr_load_ptr(&cursor->next));
	}

	return false;
}


/*
 * Copyright 2012 Abel P. Mathew
 * Copyright 2012 Samy Al Bahra
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

#ifdef __x86_64__
#define CK_BAG_MAX_N_ENTRIES (1 << 12)
#endif

static struct ck_malloc allocator;
static size_t allocator_overhead;

bool
ck_bag_init(struct ck_bag *bag,
	    size_t n_cachelines,
	    enum ck_bag_allocation_strategy as)
{
	size_t block_overhead;

	bag->avail_head = bag->avail_tail = NULL;
	bag->head = NULL;
	bag->n_entries = 0;
	bag->n_blocks = 0;
	bag->alloc_strat = as;

	/*
	 * By default, a ck_bag_block occupies two cachelines. If n_entries is less
	 * than the # of entries that can fit within one cacheline (including
	 * overhead), then bag->info.max = number of entries that can fit into a
	 * single cacheline.
	 */
	block_overhead = sizeof(struct ck_bag_block) + allocator_overhead;

	if (n_cachelines == CK_BAG_DEFAULT) {
		bag->info.max = ((CK_BAG_PAGESIZE - block_overhead) / sizeof(void *));
	} else {
		bag->info.max = ((n_cachelines * CK_MD_CACHELINE - block_overhead) / sizeof(void *));
	}

#ifdef __x86_64__
	if (bag->info.max > CK_BAG_MAX_N_ENTRIES)
		return false;
#endif

	bag->info.bytes = block_overhead + sizeof(void *) * bag->info.max;
	return true;
}

void
ck_bag_destroy(struct ck_bag *bag)
{
	struct ck_bag_block *cursor;

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
	struct ck_bag_block *cursor, *new_block, *new_block_prev, *new_tail;
	uint16_t n_entries_block;
	size_t blocks_alloc, i;
	uintptr_t next = 0;

	new_block = new_block_prev = new_tail = NULL;

	/*
	 * Blocks with available entries are stored in
	 * the bag's available list.
	 */
	cursor = bag->avail_head;
	if (cursor != NULL) {
		n_entries_block = ck_bag_block_count(cursor);
	} else {
		/* The bag is full, allocate a new set of blocks */
		if (bag->alloc_strat == CK_BAG_ALLOCATE_GEOMETRIC)
			blocks_alloc = (bag->n_blocks + 1) << 1;
		else
			blocks_alloc = 1;

		for (i = 0; i < blocks_alloc-1; i++) {
			new_block = allocator.malloc(bag->info.bytes);

			if (new_block == NULL)
				return false;

			/*
			 * First node is the tail of the Bag.
			 * Second node is the new tail of the Available list.
			 */
			if (i == 0)
				new_tail = new_block;

#ifndef __x86_64__
			new_block->next.n_entries = 0;
#endif

			new_block->next.ptr = new_block_prev;
			new_block->avail_next = new_block_prev;
			if (new_block_prev != NULL)
				new_block_prev->avail_prev = new_block;

			new_block_prev = new_block;
		}

		/*
		 * Insert entry into last allocated block.
		 * cursor is new head of available list.
		 */
		cursor = allocator.malloc(bag->info.bytes);
		cursor->avail_next = new_block;
		cursor->avail_prev = NULL;
		new_block->avail_prev = cursor;
		n_entries_block = 0;
		bag->n_blocks += blocks_alloc; /* n_blocks and n_avail_blocks? */
	}

	/* Update the available list */
	if (new_block != NULL) {
		if (bag->avail_tail != NULL) {
			cursor->avail_prev = bag->avail_tail;
			bag->avail_tail->avail_next = cursor;
		} else {
			/* Available list was previously empty */
			bag->avail_head = cursor;
		}

		bag->avail_tail = new_tail;
	} else if (n_entries_block == bag->info.max-1) {
	    /* New entry will fill up block, remove from avail list */
		    if (cursor->avail_prev != NULL)
			    cursor->avail_prev->avail_next = cursor->avail_next;

		    if (cursor->avail_next != NULL)
			    cursor->avail_next->avail_prev = cursor->avail_prev;

		    if (bag->avail_head == cursor)
			    bag->avail_head = cursor->avail_next;

		    if (bag->avail_tail == cursor)
			    bag->avail_tail = cursor->avail_prev;

		    /* For debugging purposes */
		    cursor->avail_next = NULL;
		    cursor->avail_prev = NULL;
	}

	/* Update array and block->n_entries */
	cursor->array[n_entries_block++] = entry;
	ck_pr_fence_store();

#ifdef __x86_64__
	next = ((uintptr_t)n_entries_block << 48);
#endif

	/* Update bag's list */
	if (n_entries_block == 1) {

		if (bag->head != NULL) {
#ifdef __x86_64__
			next += ((uintptr_t)(void *)ck_bag_block_next(bag->head));
#else
			next = (uintptr_t)(void *)ck_bag_block_next(bag->head);
#endif
		}

#ifndef __x86_64__
		ck_pr_store_ptr(&cursor->next.n_entries, (void *)(uintptr_t)n_entries_block);
#endif

		ck_pr_store_ptr(&cursor->next.ptr, (void *)next);
		ck_pr_store_ptr(&bag->head, cursor);
	} else {

#ifdef __x86_64__
		next += ((uintptr_t)(void *)ck_bag_block_next(cursor->next.ptr));
		ck_pr_store_ptr(&cursor->next, (void *)next);
#else
		ck_pr_store_ptr(&cursor->next.n_entries, (void *)(uintptr_t)n_entries_block);
#endif

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
		/* If a block's single entry is being removed, remove the block. */
		if (prev == NULL) {
			struct ck_bag_block *new_head = ck_bag_block_next(cursor->next.ptr);
			ck_pr_store_ptr(&bag->head, new_head);
		} else {
			uintptr_t next;
#ifdef __x86_64__
			next = ((uintptr_t)prev->next.ptr & (CK_BAG_BLOCK_ENTRIES_MASK)) |
				(uintptr_t)(void *)ck_bag_block_next(cursor->next.ptr);
#else
			next = (uintptr_t)(void *)cursor->next.ptr;
#endif
			ck_pr_store_ptr(&prev->next.ptr, (struct ck_bag_block *)next);
		}

		/* Remove block from available list */
		if (cursor->avail_prev != NULL)
			cursor->avail_prev->avail_next = cursor->avail_next;

		if (cursor->avail_next != NULL)
			cursor->avail_next->avail_prev = cursor->avail_prev;

		bag->n_blocks--;
		copy = cursor->avail_next;
	} else {
		uintptr_t next_ptr;

		copy = allocator.malloc(bag->info.bytes);
		if (copy == NULL)
			return false;

		memcpy(copy, cursor, bag->info.bytes);
		copy->array[block_index] = copy->array[--n_entries];

		next_ptr = (uintptr_t)(void *)ck_bag_block_next(copy->next.ptr);
#ifdef __x86_64__
		copy->next.ptr = (void *)(((uintptr_t)n_entries << 48) | next_ptr);
#else
		copy->next.n_entries = n_entries;
		copy->next.ptr = (struct ck_bag_block *)next_ptr;
#endif

		ck_pr_fence_store();

		if (prev == NULL) {
			ck_pr_store_ptr(&bag->head, copy);
		} else {
#ifdef __x86_64__
			uintptr_t next = ((uintptr_t)prev->next.ptr & (CK_BAG_BLOCK_ENTRIES_MASK)) |
				(uintptr_t)(void *)ck_bag_block_next(copy);
			ck_pr_store_ptr(&prev->next.ptr, (struct ck_bag_block *)next);
#else
			ck_pr_store_ptr(&prev->next.ptr, copy);
#endif
		}

		if (n_entries == bag->info.max - 1) {
			/* Block was previously fully, add to head of avail. list */
			copy->avail_next = bag->avail_head;
			copy->avail_prev = NULL;
			bag->avail_head = copy;
		}

	}

	/* Update available list. */
	if (bag->avail_head == cursor)
		bag->avail_head = copy;

	if (bag->avail_tail == cursor)
		bag->avail_tail = copy;

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


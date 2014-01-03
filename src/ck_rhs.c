/*
 * Copyright 2014 Olivier Houchard
 * Copyright 2012-2014 Samy Al Bahra.
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

#include <ck_cc.h>
#include <ck_rhs.h>
#include <ck_limits.h>
#include <ck_md.h>
#include <ck_pr.h>
#include <ck_stdint.h>
#include <stdbool.h>
#include <string.h>

#include "ck_internal.h"

#ifndef CK_RHS_PROBE_L1_SHIFT
#define CK_RHS_PROBE_L1_SHIFT 3ULL
#endif /* CK_RHS_PROBE_L1_SHIFT */

#define CK_RHS_PROBE_L1 (1 << CK_RHS_PROBE_L1_SHIFT)
#define CK_RHS_PROBE_L1_MASK (CK_RHS_PROBE_L1 - 1)

#ifndef CK_RHS_PROBE_L1_DEFAULT
#define CK_RHS_PROBE_L1_DEFAULT CK_MD_CACHELINE
#endif

#define CK_RHS_VMA_MASK ((uintptr_t)((1ULL << CK_MD_VMA_BITS) - 1))
#define CK_RHS_VMA(x)	\
	((void *)((uintptr_t)(x) & CK_RHS_VMA_MASK))

#define CK_RHS_EMPTY     NULL
#define CK_RHS_G		(1024)
#define CK_RHS_G_MASK	(CK_RHS_G - 1)

#if defined(CK_F_PR_LOAD_8) && defined(CK_F_PR_STORE_8)
#define CK_RHS_WORD          uint8_t
#define CK_RHS_WORD_MAX	    UINT8_MAX
#define CK_RHS_STORE(x, y)   ck_pr_store_8(x, y)
#define CK_RHS_LOAD(x)       ck_pr_load_8(x)
#elif defined(CK_F_PR_LOAD_16) && defined(CK_F_PR_STORE_16)
#define CK_RHS_WORD          uint16_t
#define CK_RHS_WORD_MAX	    UINT16_MAX
#define CK_RHS_STORE(x, y)   ck_pr_store_16(x, y)
#define CK_RHS_LOAD(x)       ck_pr_load_16(x)
#elif defined(CK_F_PR_LOAD_32) && defined(CK_F_PR_STORE_32)
#define CK_RHS_WORD          uint32_t
#define CK_RHS_WORD_MAX	    UINT32_MAX
#define CK_RHS_STORE(x, y)   ck_pr_store_32(x, y)
#define CK_RHS_LOAD(x)       ck_pr_load_32(x)
#else
#error "ck_rhs is not supported on your platform."
#endif

#define CK_RHS_MAX_WANTED	0xffff

enum ck_rhs_probe_behavior {
	CK_RHS_PROBE = 0,	/* Default behavior. */
	CK_RHS_PROBE_RH,		/* Short-circuit if RH slot found. */
	CK_RHS_PROBE_INSERT,	/* Short-circuit on probe bound if tombstone found. */

	CK_RHS_PROBE_ROBIN_HOOD,	/* Look for the first slot available for the entry we are about to replace, only used to internally implement Robin Hood */
	CK_RHS_PROBE_NO_RH,	/* Don't do the RH dance */
};

struct ck_rhs_entry_desc {
	void *entry;
	unsigned int probes;
	unsigned short wanted;
	CK_RHS_WORD probe_bound;
	bool in_rh;
} __attribute__ ((__aligned__(16)));

struct ck_rhs_map {
	unsigned int generation[CK_RHS_G];
	unsigned int probe_maximum;
	unsigned long mask;
	unsigned long step;
	unsigned int probe_limit;
	unsigned long n_entries;
	unsigned long capacity;
	unsigned long size;
	char offset_mask;
	struct ck_rhs_entry_desc *descs;
};

void
ck_rhs_iterator_init(struct ck_rhs_iterator *iterator)
{

	iterator->cursor = NULL;
	iterator->offset = 0;
	return;
}

bool
ck_rhs_next(struct ck_rhs *hs, struct ck_rhs_iterator *i, void **key)
{
	struct ck_rhs_map *map = hs->map;
	void *value;

	if (i->offset >= map->capacity)
		return false;

	do {
		value = map->descs[i->offset].entry;
		if (value != CK_RHS_EMPTY) {
#ifdef CK_RHS_PP
			if (hs->mode & CK_RHS_MODE_OBJECT)
				value = CK_RHS_VMA(value);
#endif
			i->offset++;
			*key = value;
			return true;
		}
	} while (++i->offset < map->capacity);

	return false;
}

void
ck_rhs_stat(struct ck_rhs *hs, struct ck_rhs_stat *st)
{
	struct ck_rhs_map *map = hs->map;

	st->n_entries = map->n_entries;
	st->probe_maximum = map->probe_maximum;
	return;
}

unsigned long
ck_rhs_count(struct ck_rhs *hs)
{

	return hs->map->n_entries;
}

static void
ck_rhs_map_destroy(struct ck_malloc *m, struct ck_rhs_map *map, bool defer)
{

	m->free(map, map->size, defer);
	return;
}

void
ck_rhs_destroy(struct ck_rhs *hs)
{

	ck_rhs_map_destroy(hs->m, hs->map, false);
	return;
}

static struct ck_rhs_map *
ck_rhs_map_create(struct ck_rhs *hs, unsigned long entries)
{
	struct ck_rhs_map *map;
	unsigned long size, n_entries, limit;

	n_entries = ck_internal_power_2(entries);
	if (n_entries < CK_RHS_PROBE_L1)
		return NULL;

	size = sizeof(struct ck_rhs_map) +
    	    (sizeof(struct ck_rhs_entry_desc) * n_entries + CK_MD_CACHELINE - 1);

	map = hs->m->malloc(size);
	if (map == NULL)
		return NULL;

	map->size = size;
	map->offset_mask = (CK_MD_CACHELINE / sizeof(struct ck_rhs_entry_desc)) - 1;
	/* We should probably use a more intelligent heuristic for default probe length. */
	limit = ck_internal_max(n_entries >> (CK_RHS_PROBE_L1_SHIFT + 2), CK_RHS_PROBE_L1_DEFAULT);
	if (limit > UINT_MAX)
		limit = UINT_MAX;

	map->probe_limit = (unsigned int)limit;
	map->probe_maximum = 0;
	map->capacity = n_entries;
	map->step = ck_internal_bsf(n_entries);
	map->mask = n_entries - 1;
	map->n_entries = 0;

	/* Align map allocation to cache line. */
	map->descs = (void *)(((uintptr_t)&map[1] +
	    CK_MD_CACHELINE - 1) & ~(CK_MD_CACHELINE - 1));
	memset(map->generation, 0, sizeof map->generation);
	memset(map->descs, 0, sizeof(*map->descs) * n_entries);

	/* Commit entries purge with respect to map publication. */
	ck_pr_fence_store();
	return map;
}

bool
ck_rhs_reset_size(struct ck_rhs *hs, unsigned long capacity)
{
	struct ck_rhs_map *map, *previous;

	previous = hs->map;
	map = ck_rhs_map_create(hs, capacity);
	if (map == NULL)
		return false;

	ck_pr_store_ptr(&hs->map, map);
	ck_rhs_map_destroy(hs->m, previous, true);
	return true;
}

bool
ck_rhs_reset(struct ck_rhs *hs)
{
	struct ck_rhs_map *previous;

	previous = hs->map;
	return ck_rhs_reset_size(hs, previous->capacity);
}

static inline unsigned long
ck_rhs_map_probe_next(struct ck_rhs_map *map,
    unsigned long offset,
    unsigned long probes)
{
	if (probes & map->offset_mask) {
		offset = (offset &~ map->offset_mask) + ((offset + 1) & map->offset_mask);
		return offset;
	} else
		return ((offset + probes) & map->mask);
}

static inline unsigned long
ck_rhs_map_probe_prev(struct ck_rhs_map *map, unsigned long offset,
    unsigned long probes)
{
	if (probes & map->offset_mask) {
		offset = (offset &~ map->offset_mask) + ((offset - 1) & 
		    map->offset_mask);
		return offset;
	} else 
		return ((offset - probes) & map->mask);
}


static inline void
ck_rhs_map_bound_set(struct ck_rhs_map *m,
    unsigned long h,
    unsigned long n_probes)
{
	unsigned long offset = h & m->mask;

	if (n_probes > m->probe_maximum)
		ck_pr_store_uint(&m->probe_maximum, n_probes);

	    if (m->descs[offset].probe_bound < n_probes) {
		if (n_probes > CK_RHS_WORD_MAX)
			n_probes = CK_RHS_WORD_MAX;

		CK_RHS_STORE(&m->descs[offset].probe_bound, n_probes);
		ck_pr_fence_store();
	}

	return;
}

static inline unsigned int
ck_rhs_map_bound_get(struct ck_rhs_map *m, unsigned long h)
{
	unsigned long offset = h & m->mask;
	unsigned int r = CK_RHS_WORD_MAX;

	r = CK_RHS_LOAD(&m->descs[offset].probe_bound);
	if (r == CK_RHS_WORD_MAX)
		r = ck_pr_load_uint(&m->probe_maximum);
	return r;
}

bool
ck_rhs_grow(struct ck_rhs *hs,
    unsigned long capacity)
{
	struct ck_rhs_map *map, *update;
	void *previous, *prev_saved;
	unsigned long k, offset, probes;

restart:
	map = hs->map;
	if (map->capacity > capacity)
		return false;

	update = ck_rhs_map_create(hs, capacity);
	if (update == NULL)
		return false;

	for (k = 0; k < map->capacity; k++) {
		unsigned long h;

		prev_saved = previous = map->descs[k].entry;
		if (previous == CK_RHS_EMPTY)
			continue;

#ifdef CK_RHS_PP
		if (hs->mode & CK_RHS_MODE_OBJECT)
			previous = CK_RHS_VMA(previous);
#endif

		h = hs->hf(previous, hs->seed);
		offset = h & update->mask;
		probes = 0;

		for (;;) {
			void **cursor = &update->descs[offset].entry;

			if (probes++ == update->probe_limit) {
				/*
				 * We have hit the probe limit, map needs to be even larger.
				 */
				ck_rhs_map_destroy(hs->m, update, false);
				capacity <<= 1;
				goto restart;
			}

			if (CK_CC_LIKELY(*cursor == CK_RHS_EMPTY)) {
				*cursor = prev_saved;
				update->n_entries++;
				update->descs[offset].probes = probes;
				ck_rhs_map_bound_set(update, h, probes);
				break;
			} else if (update->descs[offset].probes < probes) {
				void *tmp = prev_saved;
				unsigned int old_probes;
				prev_saved = previous = *cursor;
#ifdef CK_RHS_PP
				if (hs->mode & CK_RHS_MODE_OBJECT)
					previous = CK_RHS_VMA(previous);
#endif
				*cursor = tmp;
				ck_rhs_map_bound_set(update, h, probes);
				h = hs->hf(previous, hs->seed);
				old_probes = update->descs[offset].probes;
				update->descs[offset].probes = probes;
				probes = old_probes - 1;
				continue;
				}
					update->descs[offset].wanted++;

			offset = ck_rhs_map_probe_next(update, offset,  probes);
		}

	}

	ck_pr_fence_store();
	ck_pr_store_ptr(&hs->map, update);
	ck_rhs_map_destroy(hs->m, map, true);
	return true;
}

bool
ck_rhs_rebuild(struct ck_rhs *hs)
{

	return ck_rhs_grow(hs, hs->map->capacity);
}

static struct ck_rhs_entry_desc *
ck_rhs_map_probe(struct ck_rhs *hs,
    struct ck_rhs_map *map,
    unsigned long *n_probes,
    struct ck_rhs_entry_desc **priority,
    unsigned long h,
    const void *key,
    void **object,
    unsigned long probe_limit,
    enum ck_rhs_probe_behavior behavior)
{

	void *k;
	const void *compare;
	struct ck_rhs_entry_desc *cursor, *pr = NULL;
	unsigned long offset, probes, opl;

#ifdef CK_RHS_PP
	/* If we are storing object pointers, then we may leverage pointer packing. */
	unsigned long hv = 0;

	if (hs->mode & CK_RHS_MODE_OBJECT) {
		hv = (h >> 25) & CK_RHS_KEY_MASK;
		compare = CK_RHS_VMA(key);
	} else {
		compare = key;
	}
#else
	compare = key;
#endif

 	*object = NULL;
	if (behavior != CK_RHS_PROBE_ROBIN_HOOD) {
		probes = 0;
		offset = h & map->mask;
	} else {
		/* Restart from the bucket we were previously in */
		probes = *n_probes;
		offset = ck_rhs_map_probe_next(map, *priority - map->descs,
		    probes);
	}
	opl = probe_limit;
	if (behavior == CK_RHS_PROBE_INSERT)
		probe_limit = ck_rhs_map_bound_get(map, h);


	for (;;) {
		cursor = &map->descs[offset];
		if (probes++ == probe_limit) {
			if (probe_limit == opl || pr != NULL) {
				k = CK_RHS_EMPTY;
				goto leave;
			}
			/*
			 * If no eligible slot has been found yet, continue probe
			 * sequence with original probe limit.
			 */
			probe_limit = opl;
		}

		k = ck_pr_load_ptr(&cursor->entry);
		if (k == CK_RHS_EMPTY)
			goto leave;

		if ((behavior != CK_RHS_PROBE_NO_RH) &&
		    (map->descs[offset].in_rh == false) &&
		    map->descs[offset].probes <
		    probes) {
			if (pr == NULL) {
				pr = &map->descs[offset];
				*n_probes = probes;

				if (behavior == CK_RHS_PROBE_RH ||
					behavior == CK_RHS_PROBE_ROBIN_HOOD) {
					k = CK_RHS_EMPTY;
					goto leave;
				}
			}
			offset = ck_rhs_map_probe_next(map, offset, probes);
			continue;
		}


		if (behavior != CK_RHS_PROBE_ROBIN_HOOD) {
#ifdef CK_RHS_PP
			if (hs->mode & CK_RHS_MODE_OBJECT) {
				if (((uintptr_t)k >> CK_MD_VMA_BITS) != hv) {
					offset = ck_rhs_map_probe_next(map, offset, probes);
					continue;
				}

				k = CK_RHS_VMA(k);
			}
#endif

			if (k == compare)
				goto leave;

			if (hs->compare == NULL) {
				offset = ck_rhs_map_probe_next(map, offset, probes);
				continue;
			}

			if (hs->compare(k, key) == true)
				goto leave;
		}
		offset = ck_rhs_map_probe_next(map, offset, probes);
	}

leave:
	if (probes > probe_limit) {
		cursor = NULL;
	} else {
		*object = k;
	}

	if (pr == NULL)
		*n_probes = probes;

	*priority = pr;
	return cursor;
}

static inline void *
ck_rhs_marshal(unsigned int mode, const void *key, unsigned long h)
{
	void *insert;

#ifdef CK_RHS_PP
	if (mode & CK_RHS_MODE_OBJECT) {
		insert = (void *)((uintptr_t)CK_RHS_VMA(key) | ((h >> 25) << CK_MD_VMA_BITS));
	} else {
		insert = (void *)key;
	}
#else
	(void)mode;
	(void)h;
	insert = (void *)key;
#endif

	return insert;
}

bool
ck_rhs_gc(struct ck_rhs *hs)
{
	unsigned long i;
	struct ck_rhs_map *map = hs->map;

	unsigned int max_probes = 0;
	for (i = 0; i < map->capacity; i++) {
		if (map->descs[i].probes > max_probes)
			max_probes = map->descs[i].probes;
	}
	map->probe_maximum = max_probes;
	return true;
}

static void
ck_rhs_add_wanted(struct ck_rhs *hs, struct ck_rhs_entry_desc *slot, struct ck_rhs_entry_desc *old_slot, 
	unsigned long h)
{
	struct ck_rhs_map *map = hs->map;
	long offset, end_offset;
	unsigned int probes = 1;
	bool found_slot = false;

	offset = h & map->mask;
	end_offset = slot - map->descs;

	if (old_slot == NULL)
		found_slot = true;
	while (offset != end_offset) {
		if (offset == old_slot - map->descs)
			found_slot = true;
		if (found_slot && map->descs[offset].wanted < CK_RHS_MAX_WANTED)
			map->descs[offset].wanted++;
		offset = ck_rhs_map_probe_next(map, offset, probes);
		probes++;
	}
}

static unsigned long
ck_rhs_remove_wanted(struct ck_rhs *hs, struct ck_rhs_entry_desc *slot, long limit)
{
	struct ck_rhs_map *map = hs->map;
	int probes = slot->probes;
	long offset = slot - map->descs;
	bool do_remove = true;

	while (probes > 1) {
		probes--;
		offset = ck_rhs_map_probe_prev(map, offset, probes);
		if (offset == limit)
			do_remove = false;
		if (map->descs[offset].wanted != CK_RHS_MAX_WANTED && do_remove)
			map->descs[offset].wanted--;

	}
	return offset;
}

static long
ck_rhs_get_first_offset(struct ck_rhs_map *map, unsigned long offset, unsigned int probes)
{
	while (probes > 1) {
		probes--;
		offset = ck_rhs_map_probe_prev(map, offset, probes);
	}
	return (offset);
}

#define CK_RHS_MAX_RH	512

static int
ck_rhs_put_robin_hood(struct ck_rhs *hs,
    struct ck_rhs_entry_desc *orig_slot)
{
	struct ck_rhs_entry_desc *slot, *first;
	void *object, *insert;
	unsigned long orig_probes, n_probes;
	struct ck_rhs_map *map;
	unsigned long h = 0;
	long prev;
	void *key;
	long prevs[512];
	unsigned int prevs_nb = 0;

	map = hs->map;
	first = orig_slot;
	n_probes = orig_slot->probes;
restart:
	orig_probes = n_probes;
	key = first->entry;
	insert = key;
#ifdef CK_RHS_PP
	if (hs->mode & CK_RHS_MODE_OBJECT)
	    key = CK_RHS_VMA(key);
#endif
	orig_slot = first;
	orig_slot->in_rh = true;

	slot = ck_rhs_map_probe(hs, map, &n_probes, &first, h, key, &object,
	    map->probe_limit, prevs_nb == CK_RHS_MAX_RH ?
	    CK_RHS_PROBE_NO_RH : CK_RHS_PROBE_ROBIN_HOOD);

	if (slot == NULL && first == NULL) {
		if (ck_rhs_grow(hs, map->capacity << 1) == false) {
			orig_slot->in_rh = false;
			for (unsigned int i = 0; i < prevs_nb; i++) {
				orig_slot = &map->descs[prevs[i]];
				orig_slot->in_rh = false;
			}
			return -1;
		}
		return 1;
	}

	if (first != NULL) {
		int old_probes = first->probes;

		first->probes = n_probes;
		h = ck_rhs_get_first_offset(map, first - map->descs, n_probes);
		ck_rhs_map_bound_set(map, h, n_probes);
		prev = orig_slot - map->descs;
		prevs[prevs_nb++] = prev;
		n_probes = old_probes;
		goto restart;
	} else {
		/* An empty slot was found. */
		h =  ck_rhs_get_first_offset(map, slot - map->descs, n_probes);
		ck_rhs_map_bound_set(map, h, n_probes);
		ck_pr_store_ptr(&slot->entry, insert);
		ck_pr_inc_uint(&map->generation[h & CK_RHS_G_MASK]);
		ck_pr_fence_atomic_store();
		slot->probes = n_probes;
		orig_slot->in_rh = false;
		ck_rhs_add_wanted(hs, slot, orig_slot, h);
	}
	while (prevs_nb > 0) {
		prev = prevs[--prevs_nb];
		n_probes = orig_slot->probes;
		ck_pr_store_ptr(&orig_slot->entry, map->descs[prev].entry);
		h = ck_rhs_get_first_offset(map, orig_slot - map->descs, 
		    orig_slot->probes);
		ck_rhs_add_wanted(hs, orig_slot, &map->descs[prev], h);

		ck_pr_inc_uint(&map->generation[h & CK_RHS_G_MASK]);

		ck_pr_fence_atomic_store();
		orig_slot = &map->descs[prev];

		orig_slot->in_rh = false;
	}
	return 0;
}

static void
ck_rhs_do_backward_shift_delete(struct ck_rhs *hs, struct ck_rhs_entry_desc *slot)
{
	struct ck_rhs_map *map = hs->map;
	unsigned long h;
	
	h = ck_rhs_remove_wanted(hs, slot, -1);
		
	while (slot->wanted > 0) {
		unsigned long offset = 0;
		unsigned long wanted_probes = 1;
		unsigned int probe = 0;


		/* Find a successor */
		while (wanted_probes < map->probe_maximum) {
			probe = wanted_probes;
			offset = ck_rhs_map_probe_next(map, slot - map->descs, probe);
			while (probe < map->probe_maximum) {
				if (map->descs[offset].probes == probe + 1)
						break;
				probe++;
				offset = ck_rhs_map_probe_next(map, offset, probe);
			}
			if (probe < map->probe_maximum)
					break;
			wanted_probes++;
		}
		if (wanted_probes == map->probe_maximum) {
			slot->wanted = 0;
			break;
		}

		if (slot->wanted < CK_RHS_MAX_WANTED)
			slot->wanted--;
		slot->probes = wanted_probes;

		h = ck_rhs_remove_wanted(hs, &map->descs[offset], slot - map->descs);
		//HASH_FOR_SLOT(slot) = map->rh[offset].hash;
		ck_pr_store_ptr(&slot->entry, map->descs[offset].entry);
		ck_pr_inc_uint(&map->generation[h & CK_RHS_G_MASK]);
		ck_pr_fence_atomic_store();
		slot = &map->descs[offset];
	}
	ck_pr_store_ptr(&slot->entry, CK_RHS_EMPTY);
	if ((slot->probes - 1) < CK_RHS_WORD_MAX)
		CK_RHS_STORE(&map->descs[h].probe_bound, slot->probes - 1);

	slot->probes = 0;
}

bool
ck_rhs_fas(struct ck_rhs *hs,
    unsigned long h,
    const void *key,
    void **previous)
{
	struct ck_rhs_entry_desc *slot, *first;
	void *object, *insert;
	unsigned long n_probes;
	struct ck_rhs_map *map = hs->map;

	*previous = NULL;
restart:
	slot = ck_rhs_map_probe(hs, map, &n_probes, &first, h, key, &object,
	    ck_rhs_map_bound_get(map, h), CK_RHS_PROBE);

	/* Replacement semantics presume existence. */
	if (object == NULL)
		return false;

	insert = ck_rhs_marshal(hs->mode, key, h);

	if (first != NULL) {
		int ret;

		slot->in_rh = true;
		ret = ck_rhs_put_robin_hood(hs, first);
		slot->in_rh = false;
		if (CK_CC_UNLIKELY(ret == 1))
			goto restart;
		else if (CK_CC_UNLIKELY(ret != 0))
			return false;

		ck_pr_store_ptr(&first->entry, insert);
		ck_pr_inc_uint(&map->generation[h & CK_RHS_G_MASK]);
		ck_pr_fence_atomic_store();
		first->probes = n_probes;
		ck_rhs_add_wanted(hs, first, NULL, h);
		ck_rhs_do_backward_shift_delete(hs, slot);
	} else {
		ck_pr_store_ptr(&slot->entry, insert);
		slot->probes = n_probes;
	}
	*previous = object;
	return true;
}

bool
ck_rhs_set(struct ck_rhs *hs,
    unsigned long h,
    const void *key,
    void **previous)
{
	struct ck_rhs_entry_desc *slot, *first;
	void *object, *insert;
	unsigned long n_probes;
	struct ck_rhs_map *map;

	*previous = NULL;

restart:
	map = hs->map;

	slot = ck_rhs_map_probe(hs, map, &n_probes, &first, h, key, &object, map->probe_limit, CK_RHS_PROBE_INSERT);
	if (slot == NULL && first == NULL) {
		if (ck_rhs_grow(hs, map->capacity << 1) == false)
			return false;

		goto restart;
	}

	ck_rhs_map_bound_set(map, h, n_probes);
	insert = ck_rhs_marshal(hs->mode, key, h);

	if (first != NULL) {
		if (slot)
			slot->in_rh = true;
		int ret = ck_rhs_put_robin_hood(hs, first);
		if (slot)
			slot->in_rh = false;

		if (CK_CC_LIKELY(ret == 1))
			goto restart;
		if (CK_CC_LIKELY(ret == -1))
			return false;
		/* If an earlier bucket was found, then store entry there. */
		ck_pr_store_ptr(&first->entry, insert);
		first->probes = n_probes;
		/*
		 * If a duplicate key was found, then delete it after
		 * signaling concurrent probes to restart. Optionally,
		 * it is possible to install tombstone after grace
		 * period if we can guarantee earlier position of
		 * duplicate key.
		 */
		ck_rhs_add_wanted(hs, first, NULL, h); 
		if (object != NULL) {
			ck_pr_inc_uint(&map->generation[h & CK_RHS_G_MASK]);
			ck_pr_fence_atomic_store();
			ck_rhs_do_backward_shift_delete(hs, slot);
		}

	} else {
		/*
		 * If we are storing into same slot, then atomic store is sufficient
		 * for replacement.
		 */
		ck_pr_store_ptr(&slot->entry, insert);
		slot->probes = n_probes;
		if (object == NULL)
			ck_rhs_add_wanted(hs, slot, NULL, h); 
	}

	if (object == NULL) {
		map->n_entries++;
		if ((map->n_entries << 1) > map->capacity)
			ck_rhs_grow(hs, map->capacity << 1);
	}

	*previous = object;
	return true;
}

CK_CC_INLINE static bool
ck_rhs_put_internal(struct ck_rhs *hs,
    unsigned long h,
    const void *key,
    enum ck_rhs_probe_behavior behavior)
{
	struct ck_rhs_entry_desc *slot, *first;
	void *object, *insert;
	unsigned long n_probes;
	struct ck_rhs_map *map;
	struct ck_rhs_entry_desc *desc;

restart:
	map = hs->map;

	slot = ck_rhs_map_probe(hs, map, &n_probes, &first, h, key, &object,
	    map->probe_limit, behavior);

	if (slot == NULL && first == NULL) {
		if (ck_rhs_grow(hs, map->capacity << 1) == false)
			return false;

		goto restart;
	}

	/* Fail operation if a match was found. */
	if (object != NULL)
		return false;

	ck_rhs_map_bound_set(map, h, n_probes);
	insert = ck_rhs_marshal(hs->mode, key, h);

	if (first != NULL) {
		int ret = ck_rhs_put_robin_hood(hs, first);

		if (CK_CC_UNLIKELY(ret == 1))
			return ck_rhs_put_internal(hs, h, key, behavior);
		else if (CK_CC_UNLIKELY(ret == -1))
			return false;
		/* Insert key into first bucket in probe sequence. */
		ck_pr_store_ptr(&first->entry, insert);
		desc = first;
		ck_rhs_add_wanted(hs, first, NULL, h); 

	} else {
		/* An empty slot was found. */
		ck_pr_store_ptr(&slot->entry, insert);
		desc = slot;
		ck_rhs_add_wanted(hs, slot, NULL, h); 
	}
	desc->probes = n_probes;

	map->n_entries++;
	if ((map->n_entries << 1) > map->capacity)
		ck_rhs_grow(hs, map->capacity << 1);

	return true;
}

bool
ck_rhs_put(struct ck_rhs *hs,
    unsigned long h,
    const void *key)
{

	return ck_rhs_put_internal(hs, h, key, CK_RHS_PROBE_INSERT);
}

bool
ck_rhs_put_unique(struct ck_rhs *hs,
    unsigned long h,
    const void *key)
{

	return ck_rhs_put_internal(hs, h, key, CK_RHS_PROBE_RH);
}

void *
ck_rhs_get(struct ck_rhs *hs,
    unsigned long h,
    const void *key)
{
	struct ck_rhs_entry_desc *first;
	void *object;
	struct ck_rhs_map *map;
	unsigned long n_probes;
	unsigned int g, g_p, probe;
	unsigned int *generation;

	do { 
		map = ck_pr_load_ptr(&hs->map);
		generation = &map->generation[h & CK_RHS_G_MASK];
		g = ck_pr_load_uint(generation);
		probe  = ck_rhs_map_bound_get(map, h);
		ck_pr_fence_load();

		first = NULL;
		ck_rhs_map_probe(hs, map, &n_probes, &first, h, key, &object, probe, CK_RHS_PROBE_NO_RH);

		ck_pr_fence_load();
		g_p = ck_pr_load_uint(generation);
	} while (g != g_p);

	return object;
}

void *
ck_rhs_remove(struct ck_rhs *hs,
    unsigned long h,
    const void *key)
{
	struct ck_rhs_entry_desc *slot, *first;
	void *object;
	struct ck_rhs_map *map = hs->map;
	unsigned long n_probes;

	slot = ck_rhs_map_probe(hs, map, &n_probes, &first, h, key, &object,
	    ck_rhs_map_bound_get(map, h), CK_RHS_PROBE_NO_RH);
	if (object == NULL)
		return NULL;

	map->n_entries--;
	ck_rhs_do_backward_shift_delete(hs, slot);
	return object;
}

bool
ck_rhs_move(struct ck_rhs *hs,
    struct ck_rhs *source,
    ck_rhs_hash_cb_t *hf,
    ck_rhs_compare_cb_t *compare,
    struct ck_malloc *m)
{

	if (m == NULL || m->malloc == NULL || m->free == NULL || hf == NULL)
		return false;

	hs->mode = source->mode;
	hs->seed = source->seed;
	hs->map = source->map;
	hs->m = m;
	hs->hf = hf;
	hs->compare = compare;
	return true;
}

bool
ck_rhs_init(struct ck_rhs *hs,
    unsigned int mode,
    ck_rhs_hash_cb_t *hf,
    ck_rhs_compare_cb_t *compare,
    struct ck_malloc *m,
    unsigned long n_entries,
    unsigned long seed)
{

	if (m == NULL || m->malloc == NULL || m->free == NULL || hf == NULL)
		return false;

	hs->m = m;
	hs->mode = mode;
	hs->seed = seed;
	hs->hf = hf;
	hs->compare = compare;

	hs->map = ck_rhs_map_create(hs, n_entries);
	return hs->map != NULL;
}


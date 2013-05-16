/*
 * Copyright 2012-2013 Samy Al Bahra.
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
#include <ck_hs.h>
#include <ck_limits.h>
#include <ck_md.h>
#include <ck_pr.h>
#include <ck_stdint.h>
#include <stdbool.h>
#include <string.h>

#include "ck_internal.h"

#define CK_HS_PROBE_L1_SHIFT 3ULL
#define CK_HS_PROBE_L1 (1 << CK_HS_PROBE_L1_SHIFT)
#define CK_HS_PROBE_L1_MASK (CK_HS_PROBE_L1 - 1)

#ifndef CK_HS_PROBE_L1_DEFAULT
#define CK_HS_PROBE_L1_DEFAULT CK_MD_CACHELINE
#endif

#define CK_HS_EMPTY     NULL
#define CK_HS_TOMBSTONE ((void *)~(uintptr_t)0)
#define CK_HS_G		(2)
#define CK_HS_G_MASK	(CK_HS_G - 1)

struct ck_hs_map {
	unsigned int generation[CK_HS_G];
	unsigned int probe_maximum;
	unsigned long mask;
	unsigned long step;
	unsigned int probe_limit;
	unsigned int tombstones;
	unsigned long n_entries;
	unsigned long capacity;
	unsigned long size;
	void **entries;
};

void
ck_hs_iterator_init(struct ck_hs_iterator *iterator)
{

	iterator->cursor = NULL;
	iterator->offset = 0;
	return;
}

bool
ck_hs_next(struct ck_hs *hs, struct ck_hs_iterator *i, void **key)
{
	struct ck_hs_map *map = hs->map;
	void *value;

	if (i->offset >= map->capacity)
		return false;

	do {
		value = map->entries[i->offset];
		if (value != CK_HS_EMPTY && value != CK_HS_TOMBSTONE) {
#ifdef CK_HS_PP
			if (hs->mode & CK_HS_MODE_OBJECT)
				value = (void *)((uintptr_t)value & (((uintptr_t)1 << CK_MD_VMA_BITS) - 1));
#endif
			i->offset++;
			*key = value;
			return true;
		}
	} while (++i->offset < map->capacity);

	return false;
}

void
ck_hs_stat(struct ck_hs *hs, struct ck_hs_stat *st)
{
	struct ck_hs_map *map = hs->map;

	st->n_entries = map->n_entries;
	st->tombstones = map->tombstones;
	st->probe_maximum = map->probe_maximum;
	return;
}

unsigned long
ck_hs_count(struct ck_hs *hs)
{

	return hs->map->n_entries;
}

static void
ck_hs_map_destroy(struct ck_malloc *m, struct ck_hs_map *map, bool defer)
{

	m->free(map, map->size, defer);
	return;
}

void
ck_hs_destroy(struct ck_hs *hs)
{

	ck_hs_map_destroy(hs->m, hs->map, false);
	return;
}

static struct ck_hs_map *
ck_hs_map_create(struct ck_hs *hs, unsigned long entries)
{
	struct ck_hs_map *map;
	unsigned long size, n_entries, limit;

	n_entries = ck_internal_power_2(entries);
	size = sizeof(struct ck_hs_map) + (sizeof(void *) * n_entries + CK_MD_CACHELINE - 1);

	map = hs->m->malloc(size);
	if (map == NULL)
		return NULL;

	map->size = size;

	/* We should probably use a more intelligent heuristic for default probe length. */
	limit = ck_internal_max(n_entries >> (CK_HS_PROBE_L1_SHIFT + 2), CK_HS_PROBE_L1_DEFAULT);
	if (limit > UINT_MAX)
		limit = UINT_MAX;

	map->probe_limit = (unsigned int)limit;
	map->probe_maximum = 0;
	map->capacity = n_entries;
	map->step = ck_internal_bsf(n_entries);
	map->mask = n_entries - 1;
	map->n_entries = 0;

	/* Align map allocation to cache line. */
	map->entries = (void *)(((uintptr_t)(map + 1) + CK_MD_CACHELINE - 1) & ~(CK_MD_CACHELINE - 1));
	memset(map->entries, 0, sizeof(void *) * n_entries);
	memset(map->generation, 0, sizeof map->generation);

	/* Commit entries purge with respect to map publication. */
	ck_pr_fence_store();
	return map;
}

bool
ck_hs_reset_size(struct ck_hs *hs, unsigned long capacity)
{
	struct ck_hs_map *map, *previous;

	previous = hs->map;
	map = ck_hs_map_create(hs, capacity);
	if (map == NULL)
		return false;

	ck_pr_store_ptr(&hs->map, map);
	ck_hs_map_destroy(hs->m, previous, true);
	return true;
}

bool
ck_hs_reset(struct ck_hs *hs)
{
	struct ck_hs_map *previous;

	previous = hs->map;
	return ck_hs_reset_size(hs, previous->capacity);
}

static inline unsigned long
ck_hs_map_probe_next(struct ck_hs_map *map,
    unsigned long offset,
    unsigned long h,
    unsigned long level,
    unsigned long probes)
{
	unsigned long r;
	unsigned long stride;

	(void)probes;
	(void)level;

	r = h >> map->step;
	stride = (r & ~CK_HS_PROBE_L1_MASK) << 1 | (r & CK_HS_PROBE_L1_MASK);

	return (offset + (stride | CK_HS_PROBE_L1)) & map->mask;
}

bool
ck_hs_grow(struct ck_hs *hs,
    unsigned long capacity)
{
	struct ck_hs_map *map, *update;
	void **bucket, *previous;
	unsigned long k, i, j, offset, probes;

restart:
	map = hs->map;

	if (map->capacity > capacity)
		return false;

	update = ck_hs_map_create(hs, capacity);
	if (update == NULL)
		return false;

	for (k = 0; k < map->capacity; k++) {
		unsigned long h;

		previous = map->entries[k];
		if (previous == CK_HS_EMPTY || previous == CK_HS_TOMBSTONE)
			continue;

#ifdef CK_HS_PP
		if (hs->mode & CK_HS_MODE_OBJECT)
			previous = (void *)((uintptr_t)previous & (((uintptr_t)1 << CK_MD_VMA_BITS) - 1));
#endif

		h = hs->hf(previous, hs->seed);
		offset = h & update->mask;
		probes = 0;

		for (i = 0; i < update->probe_limit; i++) {
			bucket = (void *)((uintptr_t)&update->entries[offset] & ~(CK_MD_CACHELINE - 1));

			for (j = 0; j < CK_HS_PROBE_L1; j++) {
				void **cursor = bucket + ((j + offset) & (CK_HS_PROBE_L1 - 1));

				probes++;
				if (*cursor == CK_HS_EMPTY) {
					*cursor = map->entries[k];
					update->n_entries++;

					if (probes > update->probe_maximum)
						update->probe_maximum = probes;

					break;
				}
			}

			if (j < CK_HS_PROBE_L1)
				break;

			offset = ck_hs_map_probe_next(update, offset, h, i, probes);
		}

		if (i == update->probe_limit) {
			/*
			 * We have hit the probe limit, map needs to be even larger.
			 */
			ck_hs_map_destroy(hs->m, update, false);
			capacity <<= 1;
			goto restart;
		}
	}

	ck_pr_fence_store();
	ck_pr_store_ptr(&hs->map, update);
	ck_hs_map_destroy(hs->m, map, true);
	return true;
}

static void **
ck_hs_map_probe(struct ck_hs *hs,
    struct ck_hs_map *map,
    unsigned long *n_probes,
    void ***priority,
    unsigned long h,
    const void *key,
    void **object,
    unsigned long probe_limit)
{
	void **bucket, **cursor, *k;
	const void *compare;
	void **pr = NULL;
	unsigned long offset, i, j;
	unsigned long probes = 0;

#ifdef CK_HS_PP
	/* If we are storing object pointers, then we may leverage pointer packing. */
	unsigned long hv = 0;

	if (hs->mode & CK_HS_MODE_OBJECT) {
		hv = (h >> 25) & CK_HS_KEY_MASK;
		compare = (void *)((uintptr_t)key | (hv << CK_MD_VMA_BITS));
	} else {
		compare = key;
	}
#else
	compare = key;
#endif

	offset = h & map->mask;
	*object = NULL;

	for (i = 0; i < probe_limit; i++) {
		bucket = (void **)((uintptr_t)&map->entries[offset] & ~(CK_MD_CACHELINE - 1));

		for (j = 0; j < CK_HS_PROBE_L1; j++) {
			cursor = bucket + ((j + offset) & (CK_HS_PROBE_L1 - 1));
			probes++;

			k = ck_pr_load_ptr(cursor);
			if (k == CK_HS_EMPTY)
				goto leave;

			if (k == CK_HS_TOMBSTONE) {
				if (pr == NULL) {
					pr = cursor;
					*n_probes = probes;
				}

				continue;
			}

#ifdef CK_HS_PP
			if (hs->mode & CK_HS_MODE_OBJECT) {
				if (((uintptr_t)k >> CK_MD_VMA_BITS) != hv)
					continue;

				k = (void *)((uintptr_t)k & (((uintptr_t)1 << CK_MD_VMA_BITS) - 1));
			}
#endif

			if (k == compare)
				goto leave;

			if (hs->compare == NULL)
				continue;

			if (hs->compare(k, key) == true)
				goto leave;
		}

		offset = ck_hs_map_probe_next(map, offset, h, i, probes);
	}

leave:
	if (i != probe_limit) {
		*object = k;
	} else {
		cursor = NULL;
	}

	if (pr == NULL)
		*n_probes = probes;

	*priority = pr;
	return cursor;
}

bool
ck_hs_set(struct ck_hs *hs,
    unsigned long h,
    const void *key,
    void **previous)
{
	void **slot, **first, *object, *insert;
	unsigned long n_probes;
	struct ck_hs_map *map;

	*previous = NULL;

restart:
	map = hs->map;

	slot = ck_hs_map_probe(hs, map, &n_probes, &first, h, key, &object, map->probe_limit);
	if (slot == NULL && first == NULL) {
		if (ck_hs_grow(hs, map->capacity << 1) == false)
			return false;

		goto restart;
	}

#ifdef CK_HS_PP
	if (hs->mode & CK_HS_MODE_OBJECT) {
		insert = (void *)((uintptr_t)key | ((h >> 25) << CK_MD_VMA_BITS));
	} else {
		insert = (void *)key;
	}
#else
	insert = (void *)key;
#endif

	if (n_probes > map->probe_maximum)
		ck_pr_store_uint(&map->probe_maximum, n_probes);

	if (first != NULL) {
		/* If an earlier bucket was found, then store entry there. */
		ck_pr_store_ptr(first, insert);

		/*
		 * If a duplicate key was found, then delete it after
		 * signaling concurrent probes to restart. Optionally,
		 * it is possible to install tombstone after grace
		 * period if we can guarantee earlier position of
		 * duplicate key.
		 */
		if (slot != NULL && *slot != CK_HS_EMPTY) {
			ck_pr_inc_uint(&map->generation[h & CK_HS_G_MASK]);
			ck_pr_fence_atomic_store();
			ck_pr_store_ptr(slot, CK_HS_TOMBSTONE);
		}
	} else {
		/*
		 * If we are storing into same slot, then atomic store is sufficient
		 * for replacement.
		 */
		ck_pr_store_ptr(slot, insert);
	}

	if (object == NULL) {
		map->n_entries++;
		if ((map->n_entries << 1) > map->capacity)
			ck_hs_grow(hs, map->capacity << 1);
	}

	*previous = object;
	return true;
}

bool
ck_hs_put(struct ck_hs *hs,
    unsigned long h,
    const void *key)
{
	void **slot, **first, *object, *insert;
	unsigned long n_probes;
	struct ck_hs_map *map;

restart:
	map = hs->map;

	slot = ck_hs_map_probe(hs, map, &n_probes, &first, h, key, &object, map->probe_limit);
	if (slot == NULL && first == NULL) {
		if (ck_hs_grow(hs, map->capacity << 1) == false)
			return false;

		goto restart;
	}

	/* Fail operation if a match was found. */
	if (slot != NULL && *slot != CK_HS_EMPTY)
		return false;

#ifdef CK_HS_PP
	if (hs->mode & CK_HS_MODE_OBJECT) {
		insert = (void *)((uintptr_t)key | ((h >> 25) << CK_MD_VMA_BITS));
	} else {
		insert = (void *)key;
	}
#else
	insert = (void *)key;
#endif

	if (n_probes > map->probe_maximum)
		ck_pr_store_uint(&map->probe_maximum, n_probes);

	if (first != NULL) {
		/* Insert key into first bucket in probe sequence. */
		ck_pr_store_ptr(first, insert);
	} else {
		/* An empty slot was found. */
		ck_pr_store_ptr(slot, insert);
	}

	map->n_entries++;
	if ((map->n_entries << 1) > map->capacity)
		ck_hs_grow(hs, map->capacity << 1);

	return true;
}

void *
ck_hs_get(struct ck_hs *hs,
    unsigned long h,
    const void *key)
{
	void **slot, **first, *object;
	struct ck_hs_map *map;
	unsigned long n_probes;
	unsigned int g, g_p, probe;
	unsigned int *generation;

	do { 
		map = ck_pr_load_ptr(&hs->map);
		generation = &map->generation[h & CK_HS_G_MASK];
		g = ck_pr_load_uint(generation);
		probe = ck_pr_load_uint(&map->probe_maximum);
		ck_pr_fence_load();

		slot = ck_hs_map_probe(hs, map, &n_probes, &first, h, key, &object, probe);
		if (slot == NULL)
			return NULL;

		ck_pr_fence_load();
		g_p = ck_pr_load_uint(generation);
	} while (g != g_p);

	return object;
}

void *
ck_hs_remove(struct ck_hs *hs,
    unsigned long h,
    const void *key)
{
	void **slot, **first, *object;
	struct ck_hs_map *map = hs->map;
	unsigned long n_probes;

	slot = ck_hs_map_probe(hs, map, &n_probes, &first, h, key, &object, map->probe_maximum);
	if (slot == NULL || object == NULL)
		return NULL;

	ck_pr_store_ptr(slot, CK_HS_TOMBSTONE);
	map->n_entries--;
	map->tombstones++;
	return object;
}

bool
ck_hs_init(struct ck_hs *hs,
    unsigned int mode,
    ck_hs_hash_cb_t *hf,
    ck_hs_compare_cb_t *compare,
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

	hs->map = ck_hs_map_create(hs, n_entries);
	return hs->map != NULL;
}


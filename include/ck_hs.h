/*
 * Copyright 2012-2015 Samy Al Bahra.
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

#ifndef CK_HS_H
#define CK_HS_H

#include <ck_cc.h>
#include <ck_malloc.h>
#include <ck_md.h>
#include <ck_pr.h>
#include <ck_stdint.h>
#include <ck_stdbool.h>
#include <ck_stddef.h>

/*
 * Indicates a single-writer many-reader workload. Mutually
 * exclusive with CK_HS_MODE_MPMC
 */
#define CK_HS_MODE_SPMC		1

/*
 * Indicates that values to be stored are not pointers but
 * values. Allows for full precision. Mutually exclusive
 * with CK_HS_MODE_OBJECT.
 */
#define CK_HS_MODE_DIRECT	2

/*
 * Indicates that the values to be stored are pointers.
 * Allows for space optimizations in the presence of pointer
 * packing. Mutually exclusive with CK_HS_MODE_DIRECT.
 */
#define CK_HS_MODE_OBJECT	8

/*
 * Indicates a delete-heavy workload. This will reduce the
 * need for garbage collection at the cost of approximately
 * 12% to 20% increased memory usage.
 */
#define CK_HS_MODE_DELETE	16

/* Currently unsupported. */
#define CK_HS_MODE_MPMC    (void)

/*
 * Hash callback function.
 */
typedef unsigned long ck_hs_hash_cb_t(const void *, unsigned long);

/*
 * Returns pointer to object if objects are equivalent.
 */
typedef bool ck_hs_compare_cb_t(const void *, const void *);

#if defined(CK_MD_POINTER_PACK_ENABLE) && defined(CK_MD_VMA_BITS)
#define CK_HS_PP
#define CK_HS_KEY_MASK ((1U << ((sizeof(void *) * 8) - CK_MD_VMA_BITS)) - 1)
#endif

struct ck_hs_map;
struct ck_hs {
	struct ck_malloc *m;
	struct ck_hs_map *map;
	unsigned int mode;
	unsigned long seed;
	ck_hs_hash_cb_t *hf;
	ck_hs_compare_cb_t *compare;
};
typedef struct ck_hs ck_hs_t;

struct ck_hs_stat {
	unsigned long tombstones;
	unsigned long n_entries;
	unsigned int probe_maximum;
};

struct ck_hs_iterator {
	void **cursor;
	unsigned long offset;
	struct ck_hs_map *map;
};
typedef struct ck_hs_iterator ck_hs_iterator_t;

#define CK_HS_ITERATOR_INITIALIZER { NULL, 0, NULL }

/* Convenience wrapper to table hash function. */
#define CK_HS_HASH(T, F, K) F((K), (T)->seed)

/* Computes the hash of n bytes of k for the specified hash map. */
static inline unsigned long
ck_hs_hash(const struct ck_hs *hs, const void *k)
{

	return hs->hf(k, hs->seed);
}

/*
 * An extensible struct of options for `ck_hs_init_from_options`.  The
 * fields in this struct must not be rearranged and ach field must
 * have the same width as an uintptr_t.  Adding new fields to this
 * struct is forwards compatible.
 */
struct ck_hs_init_options {
	/* -- V0 options start here -- */

	/*
	 * The size of this options struct.  This is set automatically
	 * by `CK_HS_INIT_OPTIONS_INITIALIZER`, or it should be set
	 * expicitly to the size of the smallest required version of
	 * the struct. Version specific sizes are given by
	 * `CK_HS_INIT_OPTIONS_SIZE_V<version>` constants.
	 */
	uintptr_t options_size;

	/*
	 * Hash set mode.
	 */
	uintptr_t mode;

	/*
	 * Key hash function.
	 */
	ck_hs_hash_cb_t *hash_function;

	/*
	 * Key comparator function.
	 */
	ck_hs_compare_cb_t *compare;

	/*
	 * Allocator used for the hash set.
	 */
	struct ck_malloc *allocator;

	/*
	 * Initial capacity of the hash set.
	 */
	uintptr_t capacity;

	/*
	 * Hash function seed.
	 */
	uintptr_t seed;

	/* -- V1 options start here -- */

	/*
	 * When mode is CK_HS_MODE_OBJECT, then the offset in bytes
	 * from the start of the object to the start of the key within
	 * the object.  The hash and key comparator functions will
	 * then be called with the address of the embedded key rather
	 * than the object.
	 */
	uintptr_t key_offset;
};

/*
 * The zeroth version of the options struct has only the same fields
 * that `ck_hs_init` takes.
 */
#define CK_HS_INIT_OPTIONS_SIZE_V0 (7 * sizeof(uintptr_t))

/* The first version of the options struct adds `key_offset`. */
#define CK_HS_INIT_OPTIONS_SIZE_V1 (8 * sizeof(uintptr_t))

#define CK_HS_INIT_OPTIONS_INITIALIZER { .options_size = sizeof(struct ck_hs_init_options) }

typedef void *ck_hs_apply_fn_t(void *, void *);
bool ck_hs_apply(ck_hs_t *, unsigned long, const void *, ck_hs_apply_fn_t *, void *);
void ck_hs_iterator_init(ck_hs_iterator_t *);
bool ck_hs_next(ck_hs_t *, ck_hs_iterator_t *, void **);
bool ck_hs_next_spmc(ck_hs_t *, ck_hs_iterator_t *, void **);
bool ck_hs_move(ck_hs_t *, ck_hs_t *, ck_hs_hash_cb_t *,
    ck_hs_compare_cb_t *, struct ck_malloc *);
bool ck_hs_init(ck_hs_t *, unsigned int, ck_hs_hash_cb_t *,
    ck_hs_compare_cb_t *, struct ck_malloc *, unsigned long, unsigned long);
bool ck_hs_init_from_options(ck_hs_t *, const struct ck_hs_init_options *);
void *ck_hs_get(ck_hs_t *, unsigned long, const void *);
bool ck_hs_put(ck_hs_t *, unsigned long, const void *);
bool ck_hs_put_unique(ck_hs_t *, unsigned long, const void *);
bool ck_hs_set(ck_hs_t *, unsigned long, const void *, void **);
bool ck_hs_fas(ck_hs_t *, unsigned long, const void *, void **);
void *ck_hs_remove(ck_hs_t *, unsigned long, const void *);
bool ck_hs_grow(ck_hs_t *, unsigned long);
bool ck_hs_rebuild(ck_hs_t *);
bool ck_hs_gc(ck_hs_t *, unsigned long, unsigned long);
unsigned long ck_hs_count(ck_hs_t *);
bool ck_hs_reset(ck_hs_t *);
bool ck_hs_reset_size(ck_hs_t *, unsigned long);
void ck_hs_stat(ck_hs_t *, struct ck_hs_stat *);
void ck_hs_deinit(ck_hs_t *);

void ck_hs_destroy(ck_hs_t *) CK_CC_DEPRECATED("use ck_hs_deinit instead");

#endif /* CK_HS_H */

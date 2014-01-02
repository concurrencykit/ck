/*
 * Copyright 2012-2014 Samy Al Bahra
 * Copyright 2012-2014 AppNexus, Inc.
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

#ifndef _CK_BITMAP_H
#define _CK_BITMAP_H

#include <ck_cc.h>
#include <ck_limits.h>
#include <ck_pr.h>
#include <ck_stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#if defined(CK_F_PR_LOAD_64) && defined(CK_F_PR_STORE_64) && \
    defined(CK_F_PR_AND_64) && defined(CK_F_PR_OR_64)
#define CK_BITMAP_WORD  	uint64_t
#define CK_BITMAP_SHIFT 	6
#define CK_BITMAP_STORE(x, y)	ck_pr_store_64(x, y)
#define CK_BITMAP_LOAD(x)	ck_pr_load_64(x)
#define CK_BITMAP_OR(x, y)	ck_pr_or_64(x, y)
#define CK_BITMAP_AND(x, y)	ck_pr_and_64(x, y)
#elif defined(CK_F_PR_LOAD_32) && defined(CK_F_PR_STORE_32) && \
      defined(CK_F_PR_AND_32) && defined(CK_F_PR_OR_32)
#define CK_BITMAP_WORD  	uint32_t
#define CK_BITMAP_SHIFT 	5
#define CK_BITMAP_STORE(x, y)	ck_pr_store_32(x, y)
#define CK_BITMAP_LOAD(x)	ck_pr_load_32(x)
#define CK_BITMAP_OR(x, y)	ck_pr_or_32(x, y)
#define CK_BITMAP_AND(x, y)	ck_pr_and_32(x, y)
#else
#error "ck_bitmap is not supported on your platform."
#endif /* These are all internal functions. */

#define CK_BITMAP_PTR(x, i)	((x) + ((i) >> CK_BITMAP_SHIFT))
#define CK_BITMAP_BLOCK		(sizeof(CK_BITMAP_WORD) * CHAR_BIT)
#define CK_BITMAP_MASK		(CK_BITMAP_BLOCK - 1)
#define CK_BITMAP_BLOCKS(n) \
	(((n) + (CK_BITMAP_BLOCK - 1)) / CK_BITMAP_BLOCK)

#define CK_BITMAP_INSTANCE(n_entries)					\
	union {								\
		struct {						\
			unsigned int n_bits;				\
			CK_BITMAP_WORD map[CK_BITMAP_BLOCKS(n_entries)];\
		} content;						\
		struct ck_bitmap bitmap;				\
	}

#define CK_BITMAP_ITERATOR_INIT(a, b) \
	ck_bitmap_iterator_init((a), &(b)->bitmap)

#define CK_BITMAP_INIT(a, b, c) \
	ck_bitmap_init(&(a)->bitmap, (b), (c))

#define CK_BITMAP_NEXT(a, b, c) \
	ck_bitmap_next(&(a)->bitmap, (b), (c))

#define CK_BITMAP_SET_MPMC(a, b) \
	ck_bitmap_set_mpmc(&(a)->bitmap, (b))

#define CK_BITMAP_RESET_MPMC(a, b) \
	ck_bitmap_reset_mpmc(&(a)->bitmap, (b))

#define CK_BITMAP_CLEAR(a) \
	ck_bitmap_clear(&(a)->bitmap)

#define CK_BITMAP_TEST(a, b) \
	ck_bitmap_test(&(a)->bitmap, (b))

#define CK_BITMAP_BITS(a) \
	ck_bitmap_bits(&(a)->bitmap)

#define CK_BITMAP_BUFFER(a) \
	ck_bitmap_buffer(&(a)->bitmap)

#define CK_BITMAP(a) \
	(&(a)->bitmap)

struct ck_bitmap {
	unsigned int n_bits;
	CK_BITMAP_WORD map[];
};
typedef struct ck_bitmap ck_bitmap_t;

struct ck_bitmap_iterator {
	CK_BITMAP_WORD cache;
	unsigned int n_block;
	unsigned int n_bit;
	unsigned int n_limit;
};
typedef struct ck_bitmap_iterator ck_bitmap_iterator_t;

CK_CC_INLINE static unsigned int
ck_bitmap_base(unsigned int n_bits)
{

	return CK_BITMAP_BLOCKS(n_bits) * sizeof(CK_BITMAP_WORD);
}

/*
 * Returns the required number of bytes for a ck_bitmap_t object supporting the
 * specified number of bits.
 */
CK_CC_INLINE static unsigned int
ck_bitmap_size(unsigned int n_bits)
{

	return ck_bitmap_base(n_bits) + sizeof(struct ck_bitmap);
}

/*
 * Sets the bit at the offset specified in the second argument.
 */
CK_CC_INLINE static void
ck_bitmap_set_mpmc(struct ck_bitmap *bitmap, unsigned int n)
{
	CK_BITMAP_WORD mask = 0x1ULL << (n & CK_BITMAP_MASK);

	CK_BITMAP_OR(CK_BITMAP_PTR(bitmap->map, n), mask);
	return;
}

/*
 * Resets the bit at the offset specified in the second argument.
 */
CK_CC_INLINE static void
ck_bitmap_reset_mpmc(struct ck_bitmap *bitmap, unsigned int n)
{
	CK_BITMAP_WORD mask = ~(0x1ULL << (n & CK_BITMAP_MASK));

	CK_BITMAP_AND(CK_BITMAP_PTR(bitmap->map, n), mask);
	return;
}

/*
 * Resets all bits in the provided bitmap. This is not a linearized
 * operation in ck_bitmap.
 */
CK_CC_INLINE static void
ck_bitmap_clear(struct ck_bitmap *bitmap)
{
	unsigned int n_buckets = ck_bitmap_base(bitmap->n_bits) / sizeof(CK_BITMAP_WORD);
	unsigned int i;

	for (i = 0; i < n_buckets; i++)
		CK_BITMAP_STORE(&bitmap->map[i], 0);

	return;
}

/*
 * Determines whether the bit at offset specified in the
 * second argument is set.
 */
CK_CC_INLINE static bool
ck_bitmap_test(struct ck_bitmap *bitmap, unsigned int n)
{
	CK_BITMAP_WORD mask = 0x1ULL << (n & CK_BITMAP_MASK);
	CK_BITMAP_WORD block;

	block = CK_BITMAP_LOAD(CK_BITMAP_PTR(bitmap->map, n));
	return block & mask;
}

/*
 * Returns total number of bits in specified bitmap.
 */
CK_CC_INLINE static unsigned int
ck_bitmap_bits(struct ck_bitmap *bitmap)
{

	return bitmap->n_bits;
}

/*
 * Returns a pointer to the bit buffer associated
 * with the specified bitmap.
 */
CK_CC_INLINE static void *
ck_bitmap_buffer(struct ck_bitmap *bitmap)
{

	return bitmap->map;
}

/*
 * Initializes a ck_bitmap pointing to a region of memory with
 * ck_bitmap_size(n_bits) bytes. Third argument determines whether
 * default bit value is 1 (true) or 0 (false).
 */
CK_CC_INLINE static void
ck_bitmap_init(struct ck_bitmap *bitmap,
	       unsigned int n_bits,
	       bool set)
{
	unsigned int base = ck_bitmap_base(n_bits);

	bitmap->n_bits = n_bits;
	memset(bitmap->map, -(int)set, base);

	if (set == true) {
		CK_BITMAP_WORD b;

		if (n_bits < CK_BITMAP_BLOCK)
			b = n_bits;
		else
			b = n_bits % CK_BITMAP_BLOCK;

		if (b == 0)
			return;

		bitmap->map[base / sizeof(CK_BITMAP_WORD) - 1] &= (1ULL << b) - 1ULL;
	}

	return;
}


/*
 * Initialize iterator for use with provided bitmap.
 */
CK_CC_INLINE static void
ck_bitmap_iterator_init(struct ck_bitmap_iterator *i, struct ck_bitmap *bitmap)
{

	i->n_block = 0;
	i->n_bit = 0;
	i->n_limit = CK_BITMAP_BLOCKS(bitmap->n_bits);
	i->cache = CK_BITMAP_LOAD(&bitmap->map[i->n_block]);
	return;
}

/*
 * Iterate to next bit.
 */
CK_CC_INLINE static bool
ck_bitmap_next(struct ck_bitmap *bitmap,
	       struct ck_bitmap_iterator *i,
	       unsigned int *bit)
{

	/* Load next bitmap block. */
	for (;;) {
		while (i->n_bit < CK_BITMAP_BLOCK) {
			unsigned int previous = i->n_bit++;

			if (i->cache & 1) {
				*bit = previous + (CK_BITMAP_BLOCK * i->n_block);
				i->cache >>= 1;
				return true;
			}

			i->cache >>= 1;
			if (i->cache == 0)
				break;
		}

		i->n_bit = 0;
		i->n_block++;

		if (i->n_block >= i->n_limit)
			return false;

		i->cache = CK_BITMAP_LOAD(&bitmap->map[i->n_block]);
	}

	return false;
}

#endif /* _CK_BITMAP_H */


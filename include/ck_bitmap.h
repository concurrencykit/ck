/*
 * Copyright 2012 Samy Al Bahra
 * Copyright 2012 AppNexus, Inc.
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
#define CK_BITMAP_TYPE  	uint64_t
#define CK_BITMAP_SHIFT 	6
#define CK_BITMAP_STORE(x, y)	ck_pr_store_64(x, y)
#define CK_BITMAP_LOAD(x)	ck_pr_load_64(x)
#define CK_BITMAP_OR(x, y)	ck_pr_or_64(x, y)
#define CK_BITMAP_AND(x, y)	ck_pr_and_64(x, y)
#elif defined(CK_F_PR_LOAD_32) && defined(CK_F_PR_STORE_32) && \
      defined(CK_F_PR_AND_32) && defined(CK_F_PR_OR_32)
#define CK_BITMAP_TYPE  	uint32_t
#define CK_BITMAP_SHIFT 	5
#define CK_BITMAP_STORE(x, y)	ck_pr_store_32(x, y)
#define CK_BITMAP_LOAD(x)	ck_pr_load_32(x)
#define CK_BITMAP_OR(x, y)	ck_pr_or_32(x, y)
#define CK_BITMAP_AND(x, y)	ck_pr_and_32(x, y)
#else
#error "ck_bitmap is not supported on your platform."
#endif 

#define CK_BITMAP_PTR(x, i)	((x) + ((i) >> CK_BITMAP_SHIFT))
#define CK_BITMAP_MASK		(sizeof(CK_BITMAP_TYPE) * CHAR_BIT - 1)

struct ck_bitmap {
	unsigned int length;
	unsigned int n_buckets;
	CK_BITMAP_TYPE *map;
};
typedef struct ck_bitmap ck_bitmap_t;

/*
 * Returns the number of bytes that must be allocated for a bitmap
 * with the specified number of entries.
 */
CK_CC_INLINE static unsigned int
ck_bitmap_size(unsigned int entries)
{
	size_t s = sizeof(CK_BITMAP_TYPE) * CHAR_BIT;

	return ((entries + (s - 1)) / s) * sizeof(CK_BITMAP_TYPE);
}

CK_CC_INLINE static void
ck_bitmap_init(struct ck_bitmap *bitmap,
	       void *buffer,
	       unsigned int length,
	       bool set)
{

	bitmap->map = buffer;
	bitmap->length = length;
	bitmap->n_buckets = ck_bitmap_size(length) / sizeof(CK_BITMAP_TYPE);
	memset(bitmap->map, -(int)set, ck_bitmap_size(length));
	return;
}

CK_CC_INLINE static void
ck_bitmap_set_mpmc(struct ck_bitmap *bitmap, unsigned int n)
{
	CK_BITMAP_TYPE mask = 0x1ULL << (n & CK_BITMAP_MASK);
	CK_BITMAP_TYPE *map = ck_pr_load_ptr(&bitmap->map);

	CK_BITMAP_OR(CK_BITMAP_PTR(map, n), mask);
	return;
}

CK_CC_INLINE static void
ck_bitmap_reset_mpmc(struct ck_bitmap *bitmap, unsigned int n)
{
	CK_BITMAP_TYPE mask = ~(0x1ULL << (n & CK_BITMAP_MASK));
	CK_BITMAP_TYPE *map = ck_pr_load_ptr(&bitmap->map);

	CK_BITMAP_AND(CK_BITMAP_PTR(map, n), mask);
	return;
}

CK_CC_INLINE static void *
ck_bitmap_clear_mpmc(struct ck_bitmap *bitmap, void *buffer)
{
	CK_BITMAP_TYPE *pointer = ck_pr_load_ptr(&bitmap->map);
	unsigned int i;

	if (buffer != NULL) {
		if (ck_pr_cas_ptr(&bitmap->map, pointer, buffer) == true)
			return pointer;

		return buffer;
	}

	for (i = 0; i < bitmap->n_buckets; i++)
		CK_BITMAP_STORE(&pointer[i], 0);

	return NULL;
}

CK_CC_INLINE static bool
ck_bitmap_test(struct ck_bitmap *bitmap, unsigned int n)
{
	CK_BITMAP_TYPE mask = 0x1ULL << (n & CK_BITMAP_MASK);
	CK_BITMAP_TYPE *map = ck_pr_load_ptr(&bitmap->map);
	CK_BITMAP_TYPE block;

	block = CK_BITMAP_LOAD(CK_BITMAP_PTR(map, n));
	return block & mask;
}

CK_CC_INLINE static void *
ck_bitmap_buffer(struct ck_bitmap *bitmap)
{

	return bitmap->map;
}

#endif /* _CK_BITMAP_H */

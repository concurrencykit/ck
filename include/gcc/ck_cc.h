/*
 * Copyright 2009-2014 Samy Al Bahra.
 * Copyright 2014 Paul Khuong.
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

#ifndef _CK_GCC_CC_H
#define _CK_GCC_CC_H

#include <ck_md.h>

#ifdef __SUNPRO_C
#define CK_CC_UNUSED
#define CK_CC_USED
#define CK_CC_IMM
#define CK_CC_IMM_U32
#else
#define CK_CC_UNUSED __attribute__((unused))
#define CK_CC_USED   __attribute__((used))
#define CK_CC_IMM "i"
#if defined(__x86_64__) || defined(__x86__)
#define CK_CC_IMM_U32 "Z"
#define CK_CC_IMM_S32 "e"
#else
#define CK_CC_IMM_U32 CK_CC_IMM
#define CK_CC_IMM_S32 CK_CC_IMM
#endif /* __x86_64__ || __x86__ */
#endif

/*
 * If optimizations are turned on, then force inlining.
 */
#ifdef __OPTIMIZE__
#define CK_CC_INLINE CK_CC_UNUSED inline
#else
#define CK_CC_INLINE CK_CC_UNUSED
#endif

/*
 * Packed attribute.
 */
#define CK_CC_PACKED __attribute__((packed))

/*
 * Weak reference.
 */
#define CK_CC_WEAKREF __attribute__((weakref))

/*
 * Alignment attribute.
 */
#define CK_CC_ALIGN(B) __attribute__((aligned(B)))

/*
 * Cache align.
 */
#define CK_CC_CACHELINE CK_CC_ALIGN(CK_MD_CACHELINE)

/*
 * These are functions which should be avoided.
 */
#ifdef __freestanding__
#pragma GCC poison malloc free
#endif

/*
 * Branch execution hints.
 */
#define CK_CC_LIKELY(x) (__builtin_expect(!!(x), 1))
#define CK_CC_UNLIKELY(x) (__builtin_expect(!!(x), 0))

/*
 * Some compilers are overly strict regarding aliasing semantics.
 * Unfortunately, in many cases it makes more sense to pay aliasing
 * cost rather than overly expensive register spillage.
 */
#define CK_CC_ALIASED __attribute__((__may_alias__))

/*
 * Portability wrappers for bitwise ops.
 */

#define CK_F_CC_FFS
#define CK_F_CC_CLZ
#define CK_F_CC_CTZ
#define CK_F_CC_POPCOUNT

CK_CC_INLINE static int
ck_cc_ffs(unsigned int x)
{

	return __builtin_ffs(x);
}

CK_CC_INLINE static int
ck_cc_clz(unsigned int x)
{

	return __builtin_clz(x);
}

CK_CC_INLINE static int
ck_cc_ctz(unsigned int x)
{

	return __builtin_ctz(x);
}

CK_CC_INLINE static int
ck_cc_popcount(unsigned int x)
{

	return __builtin_popcount(x);
}

#endif /* _CK_GCC_CC_H */

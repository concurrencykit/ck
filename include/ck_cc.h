/*
 * Copyright 2009-2015 Samy Al Bahra.
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

#ifndef CK_CC_H
#define CK_CC_H

#if defined(__GNUC__) || defined(__SUNPRO_C)
#include "gcc/ck_cc.h"
#endif

#ifndef CK_CC_RESTRICT
#define CK_CC_RESTRICT
#endif

#define CK_CC_DECONST_PTR(X) ((void *)(uintptr_t)(X))

/*
 * Container function.
 * This relies on (compiler) implementation-defined behavior.
 */
#define CK_CC_CONTAINER(F, T, M, N)						\
	CK_CC_INLINE static T *							\
	N(F *p)									\
	{									\
		F *n = p;							\
		return (T *)(void *)(((char *)n) - ((size_t)&((T *)0)->M));	\
	}

#define CK_CC_PAD(x) union { char pad[x]; }

#ifndef CK_CC_ALIASED
#define CK_CC_ALIASED
#endif

#ifndef CK_CC_UNUSED
#define CK_CC_UNUSED
#endif

#ifndef CK_CC_USED
#define CK_CC_USED
#endif

#ifndef CK_CC_IMM
#define CK_CC_IMM
#endif

#ifndef CK_CC_PACKED
#define CK_CC_PACKED
#endif

#ifndef CK_CC_WEAKREF
#define CK_CC_WEAKREF
#endif

#ifndef CK_CC_ALIGN
#define CK_CC_ALIGN(X)
#endif

#ifndef CK_CC_CACHELINE
#define CK_CC_CACHELINE
#endif

#ifndef CK_CC_LIKELY
#define CK_CC_LIKELY(x) x
#endif

#ifndef CK_CC_UNLIKELY
#define CK_CC_UNLIKELY(x) x
#endif

#ifndef CK_CC_TYPEOF
#define CK_CC_TYPEOF(X, DEFAULT) (DEFAULT)
#endif

#ifndef CK_F_CC_FFS
#define CK_F_CC_FFS
CK_CC_INLINE static int
ck_cc_ffs(unsigned int x)
{
	unsigned int i;

	if (x == 0)
		return 0;

	for (i = 1; (x & 1) == 0; i++, x >>= 1);

	return i;
}
#endif

#ifndef CK_F_CC_CLZ
#define CK_F_CC_CLZ
#include <ck_limits.h>

CK_CC_INLINE static int
ck_cc_clz(unsigned int x)
{
	unsigned int count, i;

	for (count = 0, i = sizeof(unsigned int) * CHAR_BIT; i > 0; count++) {
		unsigned int bit = 1U << --i;

		if (x & bit)
			break;
	}

	return count;
}
#endif

#ifndef CK_F_CC_CTZ
#define CK_F_CC_CTZ
CK_CC_INLINE static int
ck_cc_ctz(unsigned int x)
{
	unsigned int i;

	if (x == 0)
		return 0;

	for (i = 0; (x & 1) == 0; i++, x >>= 1);

	return i;
}
#endif

#ifndef CK_F_CC_POPCOUNT
#define CK_F_CC_POPCOUNT
CK_CC_INLINE static int
ck_cc_popcount(unsigned int x)
{
	unsigned int acc;

	for (acc = 0; x != 0; x >>= 1)
		acc += x & 1;

	return acc;
}
#endif


#ifdef __cplusplus
#define CK_CPP_CAST(type, arg) static_cast<type>(arg)
#else
#define CK_CPP_CAST(type, arg) arg
#endif

#endif /* CK_CC_H */

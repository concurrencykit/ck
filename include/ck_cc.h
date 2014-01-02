/*
 * Copyright 2009-2014 Samy Al Bahra.
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

#ifndef _CK_CC_H
#define _CK_CC_H

#if defined(__GNUC__) || defined(__SUNPRO_C)
#include "gcc/ck_cc.h"
#endif

/*
 * Container function.
 * This relies on (compiler) implementation-defined behavior.
 */
#define CK_CC_CONTAINER(F, T, M, N)						\
	CK_CC_INLINE static T *							\
	N(F *p)									\
	{									\
		const F *n = p;							\
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

#endif /* _CK_CC_H */


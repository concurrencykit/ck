/*
 * Copyright 2009-2011 Samy Al Bahra.
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

#ifndef _CK_PR_PPC64_H
#define _CK_PR_PPC64_H

#ifndef _CK_PR_H
#error Do not include this file directly, use ck_pr.h
#endif

#include <ck_cc.h>

/*
 * The following represent supported atomic operations.
 * These operations may be emulated.
 */
#include "ck_f_pr.h"

/*
 * This bounces the hardware thread from low to medium
 * priority. I am unsure of the benefits of this approach
 * but it is used by the Linux kernel.
 */
CK_CC_INLINE static void
ck_pr_stall(void)
{

	__asm__ __volatile__("or 1, 1, 1;"
			     "or 2, 2, 2;" ::: "memory");
	return;
}

/*
 * We must assume RMO.
 */
#define CK_PR_FENCE(T, I)                               \
        CK_CC_INLINE static void                        \
        ck_pr_fence_strict_##T(void)                    \
        {                                               \
                __asm__ __volatile__(I ::: "memory");   \
        }                                               \
        CK_CC_INLINE static void ck_pr_fence_##T(void)  \
        {                                               \
                __asm__ __volatile__(I ::: "memory");   \
        }

CK_PR_FENCE(load_depends, "")
CK_PR_FENCE(store, "eieio")
CK_PR_FENCE(load, "lwsync")
CK_PR_FENCE(memory, "sync")

#undef CK_PR_FENCE

#define CK_PR_LOAD(S, M, T, C, I)				\
	CK_CC_INLINE static T					\
	ck_pr_load_##S(M *target)				\
	{							\
		T r;						\
		__asm__ __volatile__(I "%U1%X1 %0, %1"		\
					: "=r" (r)		\
					: "m"  (*(C *)target)	\
					: "memory");		\
		return (r);					\
	}

CK_PR_LOAD(ptr, void, void *, uint64_t, "ld")

#define CK_PR_LOAD_S(S, T, I) CK_PR_LOAD(S, T, T, T, I)

CK_PR_LOAD_S(64, uint64_t, "ld")
CK_PR_LOAD_S(32, uint32_t, "lwz")
CK_PR_LOAD_S(16, uint16_t, "lhz")
CK_PR_LOAD_S(8, uint8_t, "lbz")
CK_PR_LOAD_S(uint, unsigned int, "lwz")
CK_PR_LOAD_S(int, int, "lwz")
CK_PR_LOAD_S(short, short, "lhz")
CK_PR_LOAD_S(char, char, "lbz")

#undef CK_PR_LOAD_S
#undef CK_PR_LOAD

#define CK_PR_STORE(S, M, T, C, I)				\
	CK_CC_INLINE static void				\
	ck_pr_store_##S(M *target, T v)				\
	{							\
		__asm__ __volatile__(I "%U0%X0 %1, %0"		\
					: "=m" (*(C *)target)	\
					: "r" (v)		\
					: "memory");		\
		return;						\
	}

CK_PR_STORE(ptr, void, void *, uint64_t, "std")

#define CK_PR_STORE_S(S, T, I) CK_PR_STORE(S, T, T, T, I)

CK_PR_STORE_S(64, uint64_t, "std")
CK_PR_STORE_S(32, uint32_t, "stw")
CK_PR_STORE_S(16, uint16_t, "sth")
CK_PR_STORE_S(8, uint8_t, "stb")
CK_PR_STORE_S(uint, unsigned int, "stw")
CK_PR_STORE_S(int, int, "stw")
CK_PR_STORE_S(short, short, "sth")
CK_PR_STORE_S(char, char, "stb")

#undef CK_PR_STORE_S
#undef CK_PR_STORE

CK_CC_INLINE static bool
ck_pr_cas_64_value(uint64_t *target, uint64_t compare, uint64_t set, uint64_t *value)
{
	uint64_t previous;

        __asm__ __volatile__("isync;"
			     "1:"
			     "ldarx %0, 0, %1;"
			     "cmpd  0, %0, %3;"
			     "bne-  2f;"
			     "stdcx. %2, 0, %1;"
			     "bne-  1b;"
			     "2:"
			     "lwsync;"
                                : "=&r" (previous)
                                : "r"   (target),
				  "r"   (set),
                                  "r"   (compare)
                                : "memory", "cc");

        *value = previous; 
        return (previous == compare);
}

CK_CC_INLINE static bool
ck_pr_cas_ptr_value(void *target, void *compare, void *set, void *value)
{

	return ck_pr_cas_64_value(target, (uint64_t)compare, (uint64_t)set, value);
}

CK_CC_INLINE static bool
ck_pr_cas_64(uint64_t *target, uint64_t compare, uint64_t set)
{
	uint64_t previous;

        __asm__ __volatile__("isync;"
			     "1:"
			     "ldarx %0, 0, %1;"
			     "cmpd  0, %0, %3;"
			     "bne-  2f;"
			     "stdcx. %2, 0, %1;"
			     "bne-  1b;"
			     "2:"
			     "lwsync;"
                                : "=&r" (previous)
                                : "r"   (target),
				  "r"   (set),
                                  "r"   (compare)
                                : "memory", "cc");

        return (previous == compare);
}

CK_CC_INLINE static bool
ck_pr_cas_ptr(void *target, void *compare, void *set)
{

	return ck_pr_cas_64(target, (uint64_t)compare, (uint64_t)set);
}

#define CK_PR_CAS(N, T)							\
	CK_CC_INLINE static bool					\
	ck_pr_cas_##N##_value(T *target, T compare, T set, T *value)	\
	{								\
		T previous;						\
		__asm__ __volatile__("isync;"				\
				     "1:"				\
				     "lwarx %0, 0, %1;"			\
				     "cmpw  0, %0, %3;"			\
				     "bne-  2f;"			\
				     "stwcx. %2, 0, %1;"		\
				     "bne-  1b;"			\
				     "2:"				\
				     "lwsync;"				\
					: "=&r" (previous)		\
					: "r"   (target),		\
					  "r"   (set),			\
					  "r"   (compare)		\
					: "memory", "cc");		\
		*value = previous; 					\
		return (previous == compare);				\
	}								\
	CK_CC_INLINE static bool					\
	ck_pr_cas_##N(T *target, T compare, T set)			\
	{								\
		T previous;						\
		__asm__ __volatile__("isync;"				\
				     "1:"				\
				     "lwarx %0, 0, %1;"			\
				     "cmpw  0, %0, %3;"			\
				     "bne-  2f;"			\
				     "stwcx. %2, 0, %1;"		\
				     "bne-  1b;"			\
				     "2:"				\
				     "lwsync;"				\
					: "=&r" (previous)		\
					: "r"   (target),		\
					  "r"   (set),			\
					  "r"   (compare)		\
					: "memory", "cc");		\
		return (previous == compare);				\
	}

CK_PR_CAS(32, uint32_t)
CK_PR_CAS(uint, unsigned int)
CK_PR_CAS(int, int)

#undef CK_PR_CAS
#endif /* _CK_PR_PPC64_H */

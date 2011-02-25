/*
 * Copyright 2009-2011 Samy Al Bahra.
 * Copyright 2011 David Joseph.
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

#ifndef _CK_PR_H
#define _CK_PR_H

#include <ck_cc.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

#if defined(__x86_64__)
#include "gcc/x86_64/ck_pr.h"
#elif defined(__x86__)
#include "gcc/x86/ck_pr.h"
#elif defined(__sparcv9__)
#include "gcc/sparcv9/ck_pr.h"
#elif defined(__GNUC__)
#include "gcc/ck_pr.h"
#else
#error Your platform is unsupported
#endif

#define CK_PR_BIN(K, S, M, T, P, C)										\
	CK_CC_INLINE static void										\
	ck_pr_##K##_##S(M *target, T value)									\
	{													\
		T previous;											\
		C punt;												\
		punt = ck_pr_load_##S(target);									\
		previous = (T)punt;										\
		while (ck_pr_cas_##S##_value(target, (C)previous, (C)(previous P value), &previous) == false)	\
			ck_pr_stall();										\
														\
		return;												\
	}

#define CK_PR_BIN_S(K, S, T, P) CK_PR_BIN(K, S, T, T, P, T)

#if defined(CK_F_PR_LOAD_CHAR) && defined(CK_F_PR_CAS_CHAR_VALUE)

#ifndef CK_F_PR_ADD_CHAR
#define CK_F_PR_ADD_CHAR
CK_PR_BIN_S(add, char, char, +)
#endif /* CK_F_PR_ADD_CHAR */

#ifndef CK_F_PR_SUB_CHAR
#define CK_F_PR_SUB_CHAR
CK_PR_BIN_S(sub, char, char, -)
#endif /* CK_F_PR_SUB_CHAR */

#ifndef CK_F_PR_AND_CHAR
#define CK_F_PR_AND_CHAR
CK_PR_BIN_S(and, char, char, &)
#endif /* CK_F_PR_AND_CHAR */

#ifndef CK_F_PR_XOR_CHAR
#define CK_F_PR_XOR_CHAR
CK_PR_BIN_S(xor, char, char, ^)
#endif /* CK_F_PR_XOR_CHAR */

#ifndef CK_F_PR_OR_CHAR
#define CK_F_PR_OR_CHAR
CK_PR_BIN_S(or, char, char, |)
#endif /* CK_F_PR_OR_CHAR */

#endif /* CK_F_PR_LOAD_CHAR && CK_F_PR_CAS_CHAR_VALUE */

#if defined(CK_F_PR_LOAD_INT) && defined(CK_F_PR_CAS_INT_VALUE)

#ifndef CK_F_PR_ADD_INT
#define CK_F_PR_ADD_INT
CK_PR_BIN_S(add, int, int, +)
#endif /* CK_F_PR_ADD_INT */

#ifndef CK_F_PR_SUB_INT
#define CK_F_PR_SUB_INT
CK_PR_BIN_S(sub, int, int, -)
#endif /* CK_F_PR_SUB_INT */

#ifndef CK_F_PR_AND_INT
#define CK_F_PR_AND_INT
CK_PR_BIN_S(and, int, int, &)
#endif /* CK_F_PR_AND_INT */

#ifndef CK_F_PR_XOR_INT
#define CK_F_PR_XOR_INT
CK_PR_BIN_S(xor, int, int, ^)
#endif /* CK_F_PR_XOR_INT */

#ifndef CK_F_PR_OR_INT
#define CK_F_PR_OR_INT
CK_PR_BIN_S(or, int, int, |)
#endif /* CK_F_PR_OR_INT */

#endif /* CK_F_PR_LOAD_INT && CK_F_PR_CAS_INT_VALUE */

#if defined(CK_F_PR_LOAD_UINT) && defined(CK_F_PR_CAS_UINT_VALUE)

#ifndef CK_F_PR_ADD_UINT
#define CK_F_PR_ADD_UINT
CK_PR_BIN_S(add, uint, unsigned int, +)
#endif /* CK_F_PR_ADD_UINT */

#ifndef CK_F_PR_SUB_UINT
#define CK_F_PR_SUB_UINT
CK_PR_BIN_S(sub, uint, unsigned int, -)
#endif /* CK_F_PR_SUB_UINT */

#ifndef CK_F_PR_AND_UINT
#define CK_F_PR_AND_UINT
CK_PR_BIN_S(and, uint, unsigned int, &)
#endif /* CK_F_PR_AND_UINT */

#ifndef CK_F_PR_XOR_UINT
#define CK_F_PR_XOR_UINT
CK_PR_BIN_S(xor, uint, unsigned int, ^)
#endif /* CK_F_PR_XOR_UINT */

#ifndef CK_F_PR_OR_UINT
#define CK_F_PR_OR_UINT
CK_PR_BIN_S(or, uint, unsigned int, |)
#endif /* CK_F_PR_OR_UINT */

#endif /* CK_F_PR_LOAD_UINT && CK_F_PR_CAS_UINT_VALUE */

#if defined(CK_F_PR_LOAD_PTR) && defined(CK_F_PR_CAS_PTR_VALUE)

#ifndef CK_F_PR_ADD_PTR
#define CK_F_PR_ADD_PTR
CK_PR_BIN(add, ptr, void, uintptr_t, +, void *)
#endif /* CK_F_PR_ADD_PTR */

#ifndef CK_F_PR_SUB_PTR
#define CK_F_PR_SUB_PTR
CK_PR_BIN(sub, ptr, void, uintptr_t, -, void *)
#endif /* CK_F_PR_SUB_PTR */

#ifndef CK_F_PR_AND_PTR
#define CK_F_PR_AND_PTR
CK_PR_BIN(and, ptr, void, uintptr_t, &, void *)
#endif /* CK_F_PR_AND_PTR */

#ifndef CK_F_PR_XOR_PTR
#define CK_F_PR_XOR_PTR
CK_PR_BIN(xor, ptr, void, uintptr_t, ^, void *)
#endif /* CK_F_PR_XOR_PTR */

#ifndef CK_F_PR_OR_PTR
#define CK_F_PR_OR_PTR
CK_PR_BIN(or, ptr, void, uintptr_t, |, void *)
#endif /* CK_F_PR_OR_PTR */

#endif /* CK_F_PR_LOAD_PTR && CK_F_PR_CAS_PTR_VALUE */

#undef CK_PR_BIN_S
#undef CK_PR_BIN

#define CK_PR_BTX(K, S, M, T, P, C, R)								\
	CK_CC_INLINE static bool								\
	ck_pr_##K##_##S(M *target, unsigned int offset)						\
	{											\
		T previous;									\
		C punt;										\
		punt = ck_pr_load_##S(target);							\
		previous = (T)punt;								\
		while (ck_pr_cas_##S##_value(target, (C)previous,				\
			(C)(previous P (R (1 << offset))), &previous) == false)			\
				ck_pr_stall();							\
		return ((previous >> offset) & 1);						\
	}

#define CK_PR_BTX_S(K, S, T, P, R) CK_PR_BTX(K, S, T, T, P, T, R)

#if defined(CK_F_PR_LOAD_INT) && defined(CK_F_PR_CAS_INT_VALUE)

#ifndef CK_F_PR_BTC_INT
#define CK_F_PR_BTC_INT
CK_PR_BTX_S(btc, int, int, ^, 0+)
#endif /* CK_F_PR_BTC_INT */

#ifndef CK_F_PR_BTR_INT
#define CK_F_PR_BTR_INT
CK_PR_BTX_S(btr, int, int, &, ~)
#endif /* CK_F_PR_BTR_INT */

#ifndef CK_F_PR_BTS_INT
#define CK_F_PR_BTS_INT
CK_PR_BTX_S(bts, int, int, |, 0+)
#endif /* CK_F_PR_BTS_INT */

#endif /* CK_F_PR_LOAD_INT && CK_F_PR_CAS_INT_VALUE */

#if defined(CK_F_PR_LOAD_UINT) && defined(CK_F_PR_CAS_UINT_VALUE)

#ifndef CK_F_PR_BTC_UINT
#define CK_F_PR_BTC_UINT
CK_PR_BTX_S(btc, uint, unsigned int, ^, 0+)
#endif /* CK_F_PR_BTC_UINT */

#ifndef CK_F_PR_BTR_UINT
#define CK_F_PR_BTR_UINT
CK_PR_BTX_S(btr, uint, unsigned int, &, ~)
#endif /* CK_F_PR_BTR_UINT */

#ifndef CK_F_PR_BTS_UINT
#define CK_F_PR_BTS_UINT
CK_PR_BTX_S(bts, uint, unsigned int, |, 0+)
#endif /* CK_F_PR_BTS_UINT */

#endif /* CK_F_PR_LOAD_UINT && CK_F_PR_CAS_UINT_VALUE */

#if defined(CK_F_PR_LOAD_PTR) && defined(CK_F_PR_CAS_PTR_VALUE)

#ifndef CK_F_PR_BTC_PTR
#define CK_F_PR_BTC_PTR
CK_PR_BTX(btc, ptr, void, uintptr_t, ^, void *, 0+)
#endif /* CK_F_PR_BTC_PTR */

#ifndef CK_F_PR_BTR_PTR
#define CK_F_PR_BTR_PTR
CK_PR_BTX(btr, ptr, void, uintptr_t, &, void *, ~)
#endif /* CK_F_PR_BTR_PTR */

#ifndef CK_F_PR_BTS_PTR
#define CK_F_PR_BTS_PTR
CK_PR_BTX(bts, ptr, void, uintptr_t, |, void *, 0+)
#endif /* CK_F_PR_BTS_PTR */

#endif /* CK_F_PR_LOAD_PTR && CK_F_PR_CAS_PTR_VALUE */

#undef CK_PR_BTX_S
#undef CK_PR_BTX

#define CK_PR_UNARY(K, X, S, M, T)										\
	CK_CC_INLINE static void										\
	ck_pr_##K##_##S(M *target)										\
	{													\
		ck_pr_##X##_##S(target, (T)1);									\
		return;												\
	}

#define CK_PR_UNARY_Z(K, S, M, T, P, C, Z)									\
	CK_CC_INLINE static void										\
	ck_pr_##K##_##S##_zero(M *target, bool *zero)								\
	{													\
		T previous;											\
		C punt;												\
		punt = (C)ck_pr_load_##S(target);								\
		previous = (T)punt;										\
		while (ck_pr_cas_##S##_value(target, (C)previous, (C)(previous P 1), &previous) == false)	\
			ck_pr_stall();										\
		*zero = previous == Z;										\
		return;												\
	}

#define CK_PR_UNARY_S(K, X, S, M) CK_PR_UNARY(K, X, S, M, M)
#define CK_PR_UNARY_Z_S(K, S, M, P, Z) CK_PR_UNARY_Z(K, S, M, M, P, M, Z)

#if defined(CK_F_PR_LOAD_CHAR) && defined(CK_F_PR_CAS_CHAR_VALUE)

#ifndef CK_F_PR_INC_CHAR
#define CK_F_PR_INC_CHAR
CK_PR_UNARY_S(inc, add, char, char)
#endif /* CK_F_PR_INC_CHAR */

#ifndef CK_F_PR_INC_CHAR_ZERO
#define CK_F_PR_INC_CHAR_ZERO
CK_PR_UNARY_Z_S(inc, char, char, +, -1)
#endif /* CK_F_PR_INC_CHAR_ZERO */

#ifndef CK_F_PR_DEC_CHAR
#define CK_F_PR_DEC_CHAR
CK_PR_UNARY_S(dec, sub, char, char)
#endif /* CK_F_PR_DEC_CHAR */

#ifndef CK_F_PR_DEC_CHAR_ZERO
#define CK_F_PR_DEC_CHAR_ZERO
CK_PR_UNARY_Z_S(dec, char, char, -, 1)
#endif /* CK_F_PR_DEC_CHAR_ZERO */

#endif /* CK_F_PR_LOAD_CHAR && CK_F_PR_CAS_CHAR_VALUE */

#if defined(CK_F_PR_LOAD_INT) && defined(CK_F_PR_CAS_INT_VALUE)

#ifndef CK_F_PR_INC_INT
#define CK_F_PR_INC_INT
CK_PR_UNARY_S(inc, add, int, int)
#endif /* CK_F_PR_INC_INT */

#ifndef CK_F_PR_INC_INT_ZERO
#define CK_F_PR_INC_INT_ZERO
CK_PR_UNARY_Z_S(inc, int, int, +, -1)
#endif /* CK_F_PR_INC_INT_ZERO */

#ifndef CK_F_PR_DEC_INT
#define CK_F_PR_DEC_INT
CK_PR_UNARY_S(dec, sub, int, int)
#endif /* CK_F_PR_DEC_INT */

#ifndef CK_F_PR_DEC_INT_ZERO
#define CK_F_PR_DEC_INT_ZERO
CK_PR_UNARY_Z_S(dec, int, int, -, 1)
#endif /* CK_F_PR_DEC_INT_ZERO */

#endif /* CK_F_PR_LOAD_INT && CK_F_PR_CAS_INT_VALUE */

#if defined(CK_F_PR_LOAD_UINT) && defined(CK_F_PR_CAS_UINT_VALUE)

#ifndef CK_F_PR_INC_UINT
#define CK_F_PR_INC_UINT
CK_PR_UNARY_S(inc, add, uint, unsigned int)
#endif /* CK_F_PR_INC_UINT */

#ifndef CK_F_PR_INC_UINT_ZERO
#define CK_F_PR_INC_UINT_ZERO
CK_PR_UNARY_Z_S(inc, uint, unsigned int, +, UINT_MAX)
#endif /* CK_F_PR_INC_UINT_ZERO */

#ifndef CK_F_PR_DEC_UINT
#define CK_F_PR_DEC_UINT
CK_PR_UNARY_S(dec, sub, uint, unsigned int)
#endif /* CK_F_PR_DEC_UINT */

#ifndef CK_F_PR_DEC_UINT_ZERO
#define CK_F_PR_DEC_UINT_ZERO
CK_PR_UNARY_Z_S(dec, uint, unsigned int, -, 1)
#endif /* CK_F_PR_DEC_UINT_ZERO */

#endif /* CK_F_PR_LOAD_UINT && CK_F_PR_CAS_UINT_VALUE */

#if defined(CK_F_PR_LOAD_PTR) && defined(CK_F_PR_CAS_PTR_VALUE)

#ifndef CK_F_PR_INC_PTR
#define CK_F_PR_INC_PTR
CK_PR_UNARY(inc, add, ptr, void, uintptr_t)
#endif /* CK_F_PR_INC_PTR */

#ifndef CK_F_PR_INC_PTR_ZERO
#define CK_F_PR_INC_PTR_ZERO
CK_PR_UNARY_Z(inc, ptr, void, uintptr_t, +, void *, UINT_MAX)
#endif /* CK_F_PR_INC_PTR_ZERO */

#ifndef CK_F_PR_DEC_PTR
#define CK_F_PR_DEC_PTR
CK_PR_UNARY(dec, sub, ptr, void, uintptr_t)
#endif /* CK_F_PR_DEC_PTR */

#ifndef CK_F_PR_DEC_PTR_ZERO
#define CK_F_PR_DEC_PTR_ZERO
CK_PR_UNARY_Z(dec, ptr, void, uintptr_t, -, void *, 1)
#endif /* CK_F_PR_DEC_PTR_ZERO */

#endif /* CK_F_PR_LOAD_PTR && CK_F_PR_CAS_PTR_VALUE */

#undef CK_PR_UNARY_Z_S
#undef CK_PR_UNARY_S
#undef CK_PR_UNARY_Z
#undef CK_PR_UNARY

#define CK_PR_N(K, S, M, T, P, C)										\
	CK_CC_INLINE static void										\
	ck_pr_##K##_##S(M *target)										\
	{													\
		T previous;											\
		C punt;												\
		punt = (C)ck_pr_load_##S(target);								\
		previous = (T)punt;										\
		while (ck_pr_cas_##S##_value(target, (C)previous, (C)(P previous), &previous) == false)		\
			ck_pr_stall();										\
														\
		return;												\
	}

#define CK_PR_N_S(K, S, M, P) CK_PR_N(K, S, M, M, P, M)

#if defined(CK_F_PR_LOAD_CHAR) && defined(CK_F_PR_CAS_CHAR_VALUE)

#ifndef CK_F_PR_NOT_CHAR
#define CK_F_PR_NOT_CHAR
CK_PR_N_S(not, char, char, ~)
#endif /* CK_F_PR_NOT_CHAR */

#ifndef CK_F_PR_NEG_CHAR
#define CK_F_PR_NEG_CHAR
CK_PR_N_S(neg, char, char, -)
#endif /* CK_F_PR_NEG_CHAR */

#endif /* CK_F_PR_LOAD_CHAR && CK_F_PR_CAS_CHAR_VALUE */

#if defined(CK_F_PR_LOAD_INT) && defined(CK_F_PR_CAS_INT_VALUE)

#ifndef CK_F_PR_NOT_INT
#define CK_F_PR_NOT_INT
CK_PR_N_S(not, int, int, ~)
#endif /* CK_F_PR_NOT_INT */

#ifndef CK_F_PR_NEG_INT
#define CK_F_PR_NEG_INT
CK_PR_N_S(neg, int, int, -)
#endif /* CK_F_PR_NEG_INT */

#endif /* CK_F_PR_LOAD_INT && CK_F_PR_CAS_INT_VALUE */

#if defined(CK_F_PR_LOAD_UINT) && defined(CK_F_PR_CAS_UINT_VALUE)

#ifndef CK_F_PR_NOT_UINT
#define CK_F_PR_NOT_UINT
CK_PR_N_S(not, uint, unsigned int, ~)
#endif /* CK_F_PR_NOT_UINT */

#ifndef CK_F_PR_NEG_UINT
#define CK_F_PR_NEG_UINT
CK_PR_N_S(neg, uint, unsigned int, -)
#endif /* CK_F_PR_NEG_UINT */

#endif /* CK_F_PR_LOAD_UINT && CK_F_PR_CAS_UINT_VALUE */

#if defined(CK_F_PR_LOAD_PTR) && defined(CK_F_PR_CAS_PTR_VALUE)

#ifndef CK_F_PR_NOT_PTR
#define CK_F_PR_NOT_PTR
CK_PR_N(not, ptr, void, uintptr_t, ~, void *)
#endif /* CK_F_PR_NOT_PTR */

#ifndef CK_F_PR_NEG_PTR
#define CK_F_PR_NEG_PTR
CK_PR_N(neg, ptr, void, uintptr_t, -, void *)
#endif /* CK_F_PR_NEG_PTR */

#endif /* CK_F_PR_LOAD_PTR && CK_F_PR_CAS_PTR_VALUE */

#undef CK_PR_N_S
#undef CK_PR_N

#define CK_PR_FAA(S, M, T, C)											\
	CK_CC_INLINE static C											\
	ck_pr_faa_##S(M *target, T delta)									\
	{													\
		T previous;											\
		C punt;												\
		punt = (C)ck_pr_load_##S(target);								\
		previous = (T)punt;										\
		while (ck_pr_cas_##S##_value(target, (C)previous, (C)(previous + delta), &previous) == false)	\
			ck_pr_stall();										\
														\
		return ((C)previous);										\
	}	

#define CK_PR_FAS(S, M, C)											\
	CK_CC_INLINE static C											\
	ck_pr_fas_##S(M *target, C update)									\
	{													\
		C previous;											\
		previous = ck_pr_load_##S(target);								\
		while (ck_pr_cas_##S##_value(target, previous, update, &previous) == false)			\
			ck_pr_stall();										\
														\
		return (previous);										\
	}

#define CK_PR_FAA_S(S, M) CK_PR_FAA(S, M, M, M)
#define CK_PR_FAS_S(S, M) CK_PR_FAS(S, M, M)

#if defined(CK_F_PR_LOAD_CHAR) && defined(CK_F_PR_CAS_CHAR_VALUE)

#ifndef CK_F_PR_FAA_CHAR
#define CK_F_PR_FAA_CHAR
CK_PR_FAA_S(char, char)
#endif /* CK_F_PR_FAA_CHAR */

#ifndef CK_F_PR_FAS_CHAR
#define CK_F_PR_FAS_CHAR
CK_PR_FAS_S(char, char)
#endif /* CK_F_PR_FAS_CHAR */

#endif /* CK_F_PR_LOAD_CHAR && CK_F_PR_CAS_CHAR_VALUE */

#if defined(CK_F_PR_LOAD_INT) && defined(CK_F_PR_CAS_INT_VALUE)

#ifndef CK_F_PR_FAA_INT
#define CK_F_PR_FAA_INT
CK_PR_FAA_S(int, int)
#endif /* CK_F_PR_FAA_INT */

#ifndef CK_F_PR_FAS_INT
#define CK_F_PR_FAS_INT
CK_PR_FAS_S(int, int)
#endif /* CK_F_PR_FAS_INT */

#endif /* CK_F_PR_LOAD_INT && CK_F_PR_CAS_INT_VALUE */

#if defined(CK_F_PR_LOAD_UINT) && defined(CK_F_PR_CAS_UINT_VALUE)

#ifndef CK_F_PR_FAA_UINT
#define CK_F_PR_FAA_UINT
CK_PR_FAA_S(uint, unsigned int)
#endif /* CK_F_PR_FAA_UINT */

#ifndef CK_F_PR_FAS_UINT
#define CK_F_PR_FAS_UINT
CK_PR_FAS_S(uint, unsigned int)
#endif /* CK_F_PR_FAS_UINT */

#endif /* CK_F_PR_LOAD_UINT && CK_F_PR_CAS_UINT_VALUE */

#if defined(CK_F_PR_LOAD_PTR) && defined(CK_F_PR_CAS_PTR_VALUE)

#ifndef CK_F_PR_FAA_PTR
#define CK_F_PR_FAA_PTR
CK_PR_FAA(ptr, void, uintptr_t, void *)
#endif /* CK_F_PR_FAA_PTR */

#ifndef CK_F_PR_FAS_PTR
#define CK_F_PR_FAS_PTR
CK_PR_FAS(ptr, void, void *)
#endif /* CK_F_PR_FAS_PTR */

#endif /* CK_F_PR_LOAD_PTR && CK_F_PR_CAS_PTR_VALUE */

#undef CK_PR_FAA_S
#undef CK_PR_FAS_S
#undef CK_PR_FAA
#undef CK_PR_FAS

#endif /* _CK_PR_H */


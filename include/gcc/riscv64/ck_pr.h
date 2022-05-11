/*
 * Copyright 2009-2016 Samy Al Bahra.
 * Copyright 2013-2016 Olivier Houchard.
 * All rights reserved.
 * Copyright 2022 The FreeBSD Foundation.
 *
 * Portions of this software were developed by Mitchell Horne
 * under sponsorship from the FreeBSD Foundation.
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

#ifndef CK_PR_RISCV64_H
#define CK_PR_RISCV64_H

#ifndef CK_PR_H
#error Do not include this file directly, use ck_pr.h
#endif

#include <ck_cc.h>
#include <ck_md.h>

#if !defined(__riscv_xlen) || __riscv_xlen != 64
#error "only for riscv64!"
#endif

/*
 * The following represent supported atomic operations.
 * These operations may be emulated.
 */
#include "ck_f_pr.h"

/*
 * Minimum interface requirement met.
 */
#define CK_F_PR

CK_CC_INLINE static void
ck_pr_stall(void)
{

	__asm__ __volatile__("" ::: "memory");
	return;
}

/*
 * The FENCE instruction is defined in terms of predecessor and successor bits.
 * This allows for greater granularity in specifying whether reads (loads) or
 * writes (stores) may pass over either side of the fence.
 *
 * e.g. "fence r,rw" creates a barrier with acquire semantics.
 *
 * Note that atomic memory operations (AMOs) are defined by the RISC-V spec to
 * act as both a load and store memory operation (read-modify-write, in other
 * words). Thus, any of r, w, or rw will enforce ordering on an AMO.
 */
#define CK_FENCE(p, s)  __asm __volatile("fence " #p "," #s ::: "memory");
#define CK_FENCE_RW_RW  CK_FENCE(rw,rw)

#define CK_PR_FENCE(T, I)				\
	CK_CC_INLINE static void			\
	ck_pr_fence_strict_##T(void)			\
	{						\
		I;					\
	}

CK_PR_FENCE(atomic, CK_FENCE_RW_RW)
CK_PR_FENCE(atomic_store, CK_FENCE(rw,w))
CK_PR_FENCE(atomic_load, CK_FENCE(rw,r))
CK_PR_FENCE(store_atomic, CK_FENCE(w,rw))
CK_PR_FENCE(load_atomic, CK_FENCE(r,rw))
CK_PR_FENCE(store, CK_FENCE(w,w))
CK_PR_FENCE(store_load, CK_FENCE(w,r))
CK_PR_FENCE(load, CK_FENCE(r,r))
CK_PR_FENCE(load_store, CK_FENCE(r,w))
CK_PR_FENCE(memory, CK_FENCE_RW_RW)
CK_PR_FENCE(acquire, CK_FENCE(r,rw))
CK_PR_FENCE(release, CK_FENCE(rw,w))
CK_PR_FENCE(acqrel, CK_FENCE_RW_RW)
CK_PR_FENCE(lock, CK_FENCE_RW_RW)
CK_PR_FENCE(unlock, CK_FENCE_RW_RW)

#undef CK_PR_FENCE

#undef CK_FENCE_RW_RW
#undef CK_FENCE

/*
 * ck_pr_load(3)
 */
#define CK_PR_LOAD(S, M, T, I)					\
	CK_CC_INLINE static T					\
	ck_pr_md_load_##S(const M *target)			\
	{							\
		long r = 0;					\
		__asm__ __volatile__(I " %0, 0(%1)\n"		\
					: "=r" (r)		\
					: "r"  (target)		\
					: "memory");		\
		return ((T)r);					\
	}
#define CK_PR_LOAD_S(S, T, I)	CK_PR_LOAD(S, T, T, I)

CK_PR_LOAD(ptr, void, void *, "ld")
CK_PR_LOAD_S(64, uint64_t, "ld")
CK_PR_LOAD_S(32, uint32_t, "lwu")
CK_PR_LOAD_S(16, uint16_t, "lhu")
CK_PR_LOAD_S(8, uint8_t, "lbu")
CK_PR_LOAD_S(uint, unsigned int, "lwu")
CK_PR_LOAD_S(int, int, "lw")
CK_PR_LOAD_S(short, short, "lh")
CK_PR_LOAD_S(char, char, "lb")
#ifndef CK_PR_DISABLE_DOUBLE
CK_PR_LOAD_S(double, double, "ld")
#endif

#undef CK_PR_LOAD_S
#undef CK_PR_LOAD

/*
 * ck_pr_store(3)
 */
#define CK_PR_STORE(S, M, T, I)					\
	CK_CC_INLINE static void				\
	ck_pr_md_store_##S(M *target, T val)			\
	{							\
		__asm__ __volatile__(I " %1, 0(%0)"		\
					:			\
					: "r" (target),		\
					  "r" (val)		\
					: "memory");		\
	}
#define CK_PR_STORE_S(S, T, I)	CK_PR_STORE(S, T, T, I)

CK_PR_STORE(ptr, void, const void *, "sd")
CK_PR_STORE_S(64, uint64_t, "sd")
CK_PR_STORE_S(32, uint32_t, "sw")
CK_PR_STORE_S(16, uint16_t, "sh")
CK_PR_STORE_S(8, uint8_t, "sb")
CK_PR_STORE_S(uint, unsigned int, "sw")
CK_PR_STORE_S(int, int, "sw")
CK_PR_STORE_S(short, short, "sh")
CK_PR_STORE_S(char, char, "sb")
#ifndef CK_PR_DISABLE_DOUBLE
CK_PR_STORE_S(double, double, "sd")
#endif

#undef CK_PR_STORE_S
#undef CK_PR_STORE

/*
 * ck_pr_cas(3)
 *
 * NB: 'S' is to cast compare to a signed 32-bit integer, so the value will be
 * sign-extended when passed to inline asm. GCC does this sign extension
 * implicitly, while clang does not. It is necessary because lr.w sign-extends
 * the value read from memory, so compare must match that to avoid looping
 * unconditionally.
 */
#define CK_PR_CAS(N, M, T, C, S, W)					\
	CK_CC_INLINE static bool					\
	ck_pr_cas_##N##_value(M *target, T compare, T set, M *value)	\
	{								\
		T previous;						\
		int tmp;						\
		__asm__ __volatile__("1:"				\
				     "li %[tmp], 1\n"			\
				     "lr." W " %[p], %[t]\n"		\
				     "bne %[p], %[c], 2f\n"		\
				     "sc." W " %[tmp], %[s], %[t]\n"	\
				     "bnez %[tmp], 1b\n"		\
				     "2:"				\
					: [p]"=&r"   (previous),	\
					  [tmp]"=&r" (tmp),		\
					  [t]"+A"    (*(C *)target)	\
					: [s]"r"     (set),		\
					  [c]"r"     ((long)(S)compare)	\
					: "memory");			\
		*(T *)value = previous;					\
		return (tmp == 0);					\
	}								\
	CK_CC_INLINE static bool					\
	ck_pr_cas_##N(M *target, T compare, T set)			\
	{								\
		T previous;						\
		int tmp;						\
		__asm__ __volatile__("1:"				\
				     "li %[tmp], 1\n"			\
				     "lr." W " %[p], %[t]\n"		\
				     "bne %[p], %[c], 2f\n"		\
				     "sc." W " %[tmp], %[s], %[t]\n"	\
				     "bnez %[tmp], 1b\n"		\
				     "2:"				\
					: [p]"=&r"   (previous),	\
					  [tmp]"=&r" (tmp),		\
					  [t]"+A"    (*(C *)target)	\
					: [s]"r"     (set),		\
					  [c]"r"     ((long)(S)compare)	\
					: "memory");			\
		return (tmp == 0);					\
	}
#define CK_PR_CAS_S(N, T, W)	CK_PR_CAS(N, T, T, T, T, W)
#define CK_PR_CAS_32_S(N, T, W)	CK_PR_CAS(N, T, T, T, int32_t, W)

CK_PR_CAS(ptr, void, void *, uint64_t, uint64_t, "d")
CK_PR_CAS_S(64, uint64_t, "d")
CK_PR_CAS_32_S(32, uint32_t, "w")
CK_PR_CAS_32_S(uint, unsigned int, "w")
CK_PR_CAS_32_S(int, int, "w")
#ifndef CK_PR_DISABLE_DOUBLE
CK_PR_CAS_S(double, double, "d")
#endif

#undef CK_PR_CAS_S
#undef CK_PR_CAS

/*
 * ck_pr_faa(3)
 */
#define CK_PR_FAA(N, M, T, C, W)					\
	CK_CC_INLINE static T						\
	ck_pr_faa_##N(M *target, T delta)				\
	{								\
		T previous;						\
		__asm__ __volatile__("amoadd." W " %0, %2, %1\n"	\
					: "=&r" (previous),		\
					  "+A"  (*(C *)target)		\
					: "r"   (delta)			\
					: "memory");			\
		return (previous);					\
	}
#define CK_PR_FAA_S(N, T, W)	CK_PR_FAA(N, T, T, T, W)

CK_PR_FAA(ptr, void, void *, uint64_t, "d")
CK_PR_FAA_S(64, uint64_t, "d")
CK_PR_FAA_S(32, uint32_t, "w")
CK_PR_FAA_S(uint, unsigned int, "w")
CK_PR_FAA_S(int, int, "w")

#undef CK_PR_FAA_S
#undef CK_PR_FAA

/*
 * ck_pr_fas(3)
 */
#define CK_PR_FAS(N, M, T, C, W)					\
	CK_CC_INLINE static T						\
	ck_pr_fas_##N(M *target, T val)					\
	{								\
		T previous;						\
		__asm__ __volatile__("amoswap." W " %0, %2, %1\n"	\
					: "=&r" (previous),		\
					  "+A"  (*(C *)target)		\
					: "r"   (val)			\
					: "memory");			\
		return (previous);					\
	}
#define CK_PR_FAS_S(N, T, W)	CK_PR_FAS(N, T, T, T, W)

CK_PR_FAS(ptr, void, void *, uint64_t, "d")
CK_PR_FAS_S(64, uint64_t, "d")
CK_PR_FAS_S(32, uint32_t, "w")
CK_PR_FAS_S(int, int, "w")
CK_PR_FAS_S(uint, unsigned int, "w")

#undef CK_PR_FAS_S
#undef CK_PR_FAS

/*
 * ck_pr_add(3)
 */
#define CK_PR_ADD(N, M, T, C, W)					\
	CK_CC_INLINE static void					\
	ck_pr_add_##N(M *target, T val)					\
	{								\
		__asm__ __volatile__("amoadd." W " zero, %1, %0\n"	\
					: "+A" (*(C *)target)		\
					: "r"  (val)			\
					: "memory");			\
	}								\
	CK_CC_INLINE static bool					\
	ck_pr_add_##N##_is_zero(M *target, T val)			\
	{								\
		T previous;						\
		__asm__ __volatile__("amoadd." W " %0, %2, %1\n"	\
					: "=&r" (previous),		\
					  "+A"  (*(C *)target)		\
					: "r"   (val)			\
					: "memory");			\
		return (((C)previous + (C)val) == 0);			\
	}
#define CK_PR_ADD_S(N, T, W)	CK_PR_ADD(N, T, T, T, W)

CK_PR_ADD(ptr, void, void *, uint64_t, "d")
CK_PR_ADD_S(64, uint64_t, "d")
CK_PR_ADD_S(32, uint32_t, "w")
CK_PR_ADD_S(uint, unsigned int, "w")
CK_PR_ADD_S(int, int, "w")

#undef CK_PR_ADD_S
#undef CK_PR_ADD

/*
 * ck_pr_inc(3)
 *
 * Implemented in terms of ck_pr_add(3); RISC-V has no atomic inc or dec
 * instructions.
 */
#define CK_PR_INC(N, M, T, W)						\
	CK_CC_INLINE static void					\
	ck_pr_inc_##N(M *target)					\
	{								\
		ck_pr_add_##N(target, (T)1);				\
	}								\
	CK_CC_INLINE static bool					\
	ck_pr_inc_##N##_is_zero(M *target)				\
	{								\
		return (ck_pr_add_##N##_is_zero(target, (T)1));		\
	}
#define CK_PR_INC_S(N, T, W)	CK_PR_INC(N, T, T, W)

CK_PR_INC(ptr, void, void *, "d")
CK_PR_INC_S(64, uint64_t, "d")
CK_PR_INC_S(32, uint32_t, "w")
CK_PR_INC_S(uint, unsigned int, "w")
CK_PR_INC_S(int, int, "w")

#undef CK_PR_INC_S
#undef CK_PR_INC

/*
 * ck_pr_sub(3)
 */
#define CK_PR_SUB(N, M, T, C, W)					\
	CK_CC_INLINE static void					\
	ck_pr_sub_##N(M *target, T val)					\
	{								\
		__asm__ __volatile__("amoadd." W " zero, %1, %0\n"	\
					: "+A" (*(C *)target)		\
					: "r"  (-(C)val)		\
					: "memory");			\
	}								\
	CK_CC_INLINE static bool					\
	ck_pr_sub_##N##_is_zero(M *target, T val)			\
	{								\
		T previous;						\
		__asm__ __volatile__("amoadd." W " %0, %2, %1\n"	\
					: "=&r" (previous),		\
					  "+A"  (*(C *)target)		\
					: "r"   (-(C)val)		\
					: "memory");			\
		return (((C)previous - (C)val) == 0);			\
	}
#define CK_PR_SUB_S(N, T, W)	CK_PR_SUB(N, T, T, T, W)

CK_PR_SUB(ptr, void, void *, uint64_t, "d")
CK_PR_SUB_S(64, uint64_t, "d")
CK_PR_SUB_S(32, uint32_t, "w")
CK_PR_SUB_S(uint, unsigned int, "w")
CK_PR_SUB_S(int, int, "w")

#undef CK_PR_SUB_S
#undef CK_PR_SUB

/*
 * ck_pr_dec(3)
 */
#define CK_PR_DEC(N, M, T, W)						\
	CK_CC_INLINE static void					\
	ck_pr_dec_##N(M *target)					\
	{								\
		ck_pr_sub_##N(target, (T)1);				\
	}								\
	CK_CC_INLINE static bool					\
	ck_pr_dec_##N##_is_zero(M *target)				\
	{								\
		return (ck_pr_sub_##N##_is_zero(target, (T)1));		\
	}
#define CK_PR_DEC_S(N, T, W)	CK_PR_DEC(N, T, T, W)

CK_PR_DEC(ptr, void, void *, "d")
CK_PR_DEC_S(64, uint64_t, "d")
CK_PR_DEC_S(32, uint32_t, "w")
CK_PR_DEC_S(uint, unsigned int, "w")
CK_PR_DEC_S(int, int, "w")

#undef CK_PR_DEC_S
#undef CK_PR_DEC

/*
 * ck_pr_neg(3)
 */
#define CK_PR_NEG(N, M, T, C, W)					\
	CK_CC_INLINE static void					\
	ck_pr_neg_##N(M *target)					\
	{								\
		__asm__ __volatile__("1:"				\
				     "lr." W " t0, %0\n"		\
				     "sub t0, zero, t0\n"		\
				     "sc." W " t1, t0, %0\n"		\
				     "bnez t1, 1b\n"			\
					: "+A" (*(C *)target)		\
					:				\
					: "t0", "t1", "memory");	\
	}
#define CK_PR_NEG_S(N, T, W)	CK_PR_NEG(N, T, T, T, W)

CK_PR_NEG(ptr, void, void *, uint64_t, "d")
CK_PR_NEG_S(64, uint64_t, "d")
CK_PR_NEG_S(32, uint32_t, "w")
CK_PR_NEG_S(uint, unsigned int, "w")
CK_PR_NEG_S(int, int, "w")

#undef CK_PR_NEG_S
#undef CK_PR_NEG

/*
 * ck_pr_not(3)
 */
#define CK_PR_NOT(N, M, T, C, W)					\
	CK_CC_INLINE static void					\
	ck_pr_not_##N(M *target)					\
	{								\
		__asm__ __volatile__("1:"				\
				     "lr." W " t0, %0\n"		\
				     "not t0, t0\n"			\
				     "sc." W " t1, t0, %0\n"		\
				     "bnez t1, 1b\n"			\
					: "+A" (*(C *)target)		\
					:				\
					: "t0", "t1", "memory");	\
	}
#define CK_PR_NOT_S(N, T, W)	CK_PR_NOT(N, T, T, T, W)

CK_PR_NOT(ptr, void, void *, uint64_t, "d")
CK_PR_NOT_S(64, uint64_t, "d")
CK_PR_NOT_S(32, uint32_t, "w")
CK_PR_NOT_S(uint, unsigned int, "w")
CK_PR_NOT_S(int, int, "w")

#undef CK_PR_NOT_S
#undef CK_PR_NOT

/*
 * ck_pr_and(3), ck_pr_or(3), and ck_pr_xor(3)
 */
#define CK_PR_BINARY(O, N, M, T, C, I, W)			\
	CK_CC_INLINE static void				\
	ck_pr_##O##_##N(M *target, T delta)			\
	{							\
		__asm__ __volatile__(I "." W " zero, %1, %0\n"	\
					: "+A" (*(C *)target)	\
					: "r"  (delta)		\
					: "memory");		\
	}

CK_PR_BINARY(and, ptr, void, void *, uint64_t, "amoand", "d")
CK_PR_BINARY(or, ptr, void, void *, uint64_t, "amoor", "d")
CK_PR_BINARY(xor, ptr, void, void *, uint64_t, "amoxor", "d")

#define CK_PR_BINARY_S(S, T, W)				\
        CK_PR_BINARY(and, S, T, T, T, "amoand", W)	\
        CK_PR_BINARY(or, S, T, T, T, "amoor", W)	\
        CK_PR_BINARY(xor, S, T, T, T, "amoxor", W)	\

CK_PR_BINARY_S(64, uint64_t, "d")
CK_PR_BINARY_S(32, uint32_t, "w")
CK_PR_BINARY_S(uint, unsigned int, "w")
CK_PR_BINARY_S(int, int, "w")

#undef CK_PR_BINARY_S
#undef CK_PR_BINARY

/*
 * ck_pr_btc(3), ck_pr_btr(3), and ck_pr_bts(3)
 */
#define CK_PR_BTX(K, S, I, W, M, C, O)					\
	CK_CC_INLINE static bool					\
	ck_pr_##K##_##S(M *target, unsigned int idx)			\
	{								\
		C ret;							\
		C mask = (C)0x1 << idx;					\
		__asm__ __volatile__(I "." W " %1, %2, %0\n"		\
					: "+A" (*(C *)target),		\
					  "=r" (ret)			\
					: "r"  (O(mask))		\
					: "memory", "cc");		\
		return ((ret & mask) != 0);				\
	}

#define CK_PR_BTC(S, W, M, C)	CK_PR_BTX(btc, S, "amoxor", W, M, C, 0+)
#define CK_PR_BTC_S(S, W, T)	CK_PR_BTC(S, W, T, T)

CK_PR_BTC(ptr, "d", void, uint64_t)
CK_PR_BTC_S(64, "d", uint64_t)
CK_PR_BTC_S(32, "w", uint32_t)
CK_PR_BTC_S(uint, "w", unsigned int)
CK_PR_BTC_S(int, "w", int)

#undef CK_PR_BTC_S
#undef CK_PR_BTC

#define CK_PR_BTR(S, W, M, C)	CK_PR_BTX(btr, S, "amoand", W, M, C, ~)
#define CK_PR_BTR_S(S, W, T)	CK_PR_BTR(S, W, T, T)

CK_PR_BTR(ptr, "d", void, uint64_t)
CK_PR_BTR_S(64, "d", uint64_t)
CK_PR_BTR_S(32, "w", uint32_t)
CK_PR_BTR_S(uint, "w", unsigned int)
CK_PR_BTR_S(int, "w", int)

#undef CK_PR_BTR_S
#undef CK_PR_BTR

#define CK_PR_BTS(S, W, M, C)	CK_PR_BTX(bts, S, "amoor", W, M, C, 0+)
#define CK_PR_BTS_S(S, W, T)	CK_PR_BTS(S, W, T, T)

CK_PR_BTS(ptr, "d", void, uint64_t)
CK_PR_BTS_S(64, "d", uint64_t)
CK_PR_BTS_S(32, "w", uint32_t)
CK_PR_BTS_S(uint, "w", unsigned int)
CK_PR_BTS_S(int, "w", int)

#undef CK_PR_BTS_S
#undef CK_PR_BTS

#undef CK_PR_BTX

#endif /* CK_PR_RISCV64_H */

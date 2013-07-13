/*
 * Copyright 2013 Samy Al Bahra.
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

#ifndef _CK_ELIDE_H
#define _CK_ELIDE_H

#include <ck_cc.h>
#include <ck_pr.h>

/*
 * Defines an elision implementation according to the following variables:
 *     N - Namespace of elision implementation.
 *     T - Typename of mutex.
 *   L_P - Lock predicate, returns false if resource is available.
 *     L - Function to call if resource is unavailable of transaction aborts.
 *   U_P - Unlock predicate, returns false if elision failed.
 *     U - Function to call if transaction failed.
 */
#ifdef CK_F_PR_RTM
#define CK_ELIDE_PROTOTYPE(N, T, L_P, L, U_P, U)			\
	CK_CC_INLINE static void					\
	ck_elide_##N##_lock(T *lock)					\
	{								\
									\
		if (ck_pr_rtm_begin() != CK_PR_RTM_STARTED) {		\
			L(lock);					\
			return;						\
		}							\
									\
		if (L_P(lock) == true)					\
			ck_pr_rtm_abort(0xFF);				\
									\
		return;							\
	}								\
	CK_CC_INLINE static void					\
	ck_elide_##N##_unlock(T *lock)					\
	{								\
									\
		if (U_P(lock) == false) {				\
			ck_pr_rtm_end();				\
		} else {						\
			U(lock);					\
		}							\
									\
		return;							\
	}
#define CK_ELIDE_TRYLOCK_PROTOTYPE(N, T, TL_P, TL)			\
	CK_CC_INLINE static bool					\
	ck_elide_##N##_trylock(T *lock)					\
	{								\
									\
		if (ck_pr_rtm_begin() != CK_PR_RTM_STARTED)		\
			return false;					\
									\
		if (TL_P(lock) == true)					\
			ck_pr_rtm_abort(0xFF);				\
									\
		return true;						\
	}
#else
#define CK_ELIDE_PROTOTYPE(N, T, L_P, U_P, L, U)			\
	CK_CC_INLINE static void					\
	ck_elide_##N##_lock(T *lock)					\
	{								\
									\
		L(lock);						\
		return;							\
	}								\
	CK_CC_INLINE static void					\
	ck_elide_##N##_unlock(T *lock)					\
	{								\
									\
		U(lock);						\
		return;							\
	}
#define CK_ELIDE_TRYLOCK_PROTOTYPE(N, T, TL_P, TL)			\
	CK_CC_INLINE static bool					\
	ck_elide_##N##_trylock(T *lock)					\
	{								\
									\
		return TL_P(lock);					\
	}
#endif /* CK_F_PR_RTM */

/*
 * Best-effort elision lock operations. First argument is name (N)
 * associated with implementation and the second is a pointer to
 * the type specified above (T).
 */
#define CK_ELIDE_LOCK(NAME, LOCK)	ck_elide_##NAME##_lock(LOCK)	
#define CK_ELIDE_UNLOCK(NAME, LOCK)	ck_elide_##NAME##_unlock(LOCK)
#define CK_ELIDE_TRYLOCK(NAME, LOCK)	ck_elide_##NAME##_trylock(LOCK)

#endif /* _CK_ELIDE_H */


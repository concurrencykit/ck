/*
 * Copyright 2013 Samy Al Bahra.
 * Copyright 2013 Brendon Scheinman.
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

#ifndef _CK_COHORT_H
#define _CK_COHORT_H

#include <ck_pr.h>
#include <stdbool.h>
#include <stddef.h>


#define RELEASE_STATE_GLOBAL		0
#define RELEASE_STATE_LOCAL			1

#define DEFAULT_LOCAL_PASS_LIMIT	10

#define CK_CREATE_COHORT(N, TG, TL)											\
	struct ck_cohort_##N {													\
		TG *global_lock;													\
		TL *local_lock;														\
		unsigned int release_state;											\
		unsigned int waiting_threads;										\
		unsigned int acquire_count;											\
		unsigned int local_pass_limit;										\
	};																		\
																			\
	CK_CC_INLINE static void												\
	ck_cohort_##N##_init(struct ck_cohort_##N *cohort,						\
			TG *global_lock, TL *local_lock)								\
	{																		\
		ck_pr_store_ptr(&cohort->global_lock, global_lock);					\
		ck_pr_store_ptr(&cohort->local_lock, local_lock);					\
		ck_pr_store_uint(&cohort->release_state, RELEASE_STATE_GLOBAL);		\
		ck_pr_store_uint(&cohort->waiting_threads, 0);						\
		ck_pr_store_uint(&cohort->acquire_count, 0);						\
		ck_pr_store_uint(&cohort->local_pass_limit,							\
			DEFAULT_LOCAL_PASS_LIMIT);										\
		return;																\
	}																		\
																			\
	CK_CC_INLINE static void												\
	ck_cohort_##N##_lock(struct ck_cohort_##N *cohort)						\
	{																		\
		unsigned int release_state;											\
																			\
		/*																	\
		* Fence memory right after this increment to maximize the chance	\
		* that another releasing thread will hold onto the global lock		\
		* if possible.  If the timing works out such that it relinquishes	\
		* the global lock when it doesn't have to, that's a potential		\
		* performance hit but it doesn't violate correctness.				\
		*/																	\
		ck_pr_inc_uint(&cohort->waiting_threads);							\
		ck_pr_fence_memory();												\
																			\
		TL##_lock((TL *) ck_pr_load_ptr(&cohort->local_lock));				\
		ck_pr_dec_uint(&cohort->waiting_threads);							\
																			\
		release_state = ck_pr_load_uint(&cohort->release_state);			\
		if (release_state == RELEASE_STATE_GLOBAL) {						\
			TG##_lock((TG *) ck_pr_load_ptr(&cohort->global_lock));			\
			ck_pr_store_uint(&cohort->release_state, RELEASE_STATE_LOCAL);	\
		}																	\
																			\
		/*																	\
		* We can increment this count any time between now and when			\
		* we release the lock, but we may as well do it now because we're	\
		* about to fence memory anyway.										\
		*/																	\
		ck_pr_inc_uint(&cohort->acquire_count);								\
																			\
		ck_pr_fence_memory();												\
		return;																\
	}																		\
																			\
	CK_CC_INLINE static bool												\
	ck_cohort_##N##_may_pass_local(struct ck_cohort_##N *cohort)			\
	{																		\
		return ck_pr_load_uint(&cohort->acquire_count) <					\
			ck_pr_load_uint(&cohort->local_pass_limit);						\
	}																		\
																			\
	CK_CC_INLINE static void												\
	ck_cohort_##N##_unlock(struct ck_cohort_##N *cohort)					\
	{																		\
		if (ck_pr_load_uint(&cohort->waiting_threads) > 0					\
				&& ck_cohort_##N##_may_pass_local(cohort)) {				\
			ck_pr_store_uint(&cohort->release_state, RELEASE_STATE_LOCAL);	\
		} else {															\
			TG##_unlock((TG *) ck_pr_load_ptr(&cohort->global_lock));		\
			ck_pr_store_uint(&cohort->release_state, RELEASE_STATE_GLOBAL);	\
			ck_pr_store_uint(&cohort->acquire_count, 0);					\
		}																	\
																			\
		TL##_unlock((TL *) ck_pr_load_ptr(&cohort->local_lock));			\
																			\
		ck_pr_fence_memory();												\
		return;																\
	}


#define CK_COHORT_INITIALIZER\
	{ NULL, NULL, RELEASE_STATE_GLOBAL, 0, 0, DEFAULT_LOCAL_PASS_LIMIT }


#endif /* _CK_COHORT_H */
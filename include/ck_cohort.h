#ifndef _CK_COHORT_H
#define _CK_COHORT_H

#include <ck_pr.h>
#include <ck_spinlock.h>
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


CK_CREATE_COHORT(test, ck_spinlock_fas_t, ck_spinlock_fas_t)


#endif /* _CK_COHORT_H */
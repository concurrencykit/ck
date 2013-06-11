/*
 * Copyright 2010-2013 Samy Al Bahra.
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

#ifndef _CK_SPINLOCK_H
#define _CK_SPINLOCK_H

#include <ck_backoff.h>
#include <ck_cc.h>
#include <ck_limits.h>
#include <ck_md.h>
#include <ck_pr.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * On tested x86, x86_64, PPC64 and SPARC64 targets,
 * ck_spinlock_fas proved to have lowest latency
 * in fast path testing or negligible degradation
 * from faster but less robust implementations.
 */
#define CK_SPINLOCK_INITIALIZER CK_SPINLOCK_FAS_INITIALIZER
#define ck_spinlock_t           ck_spinlock_fas_t
#define ck_spinlock_init(x)     ck_spinlock_fas_init(x)
#define ck_spinlock_lock(x)     ck_spinlock_fas_lock(x)
#define ck_spinlock_lock_eb(x)  ck_spinlock_fas_lock_eb(x)
#define ck_spinlock_unlock(x)   ck_spinlock_fas_unlock(x)
#define ck_spinlock_locked(x)   ck_spinlock_fas_locked(x)
#define ck_spinlock_trylock(x)  ck_spinlock_fas_trylock(x)

#ifndef CK_F_SPINLOCK_ANDERSON
#define CK_F_SPINLOCK_ANDERSON
/*
 * This is an implementation of Anderson's array-based queuing lock.
 */
struct ck_spinlock_anderson_thread {
	unsigned int locked;
	unsigned int position;
};
typedef struct ck_spinlock_anderson_thread ck_spinlock_anderson_thread_t;

struct ck_spinlock_anderson {
	struct ck_spinlock_anderson_thread *slots;
	unsigned int count;
	unsigned int wrap;
	unsigned int mask;
	char pad[CK_MD_CACHELINE - sizeof(unsigned int) * 3 - sizeof(void *)];
	unsigned int next;
};
typedef struct ck_spinlock_anderson ck_spinlock_anderson_t;

CK_CC_INLINE static void
ck_spinlock_anderson_init(struct ck_spinlock_anderson *lock,
    struct ck_spinlock_anderson_thread *slots,
    unsigned int count)
{
	unsigned int i;

	slots[0].locked = false;
	slots[0].position = 0;
	for (i = 1; i < count; i++) {
		slots[i].locked = true;
		slots[i].position = i;
	}

	lock->slots = slots;
	lock->count = count;
	lock->mask = count - 1;
	lock->next = 0;

	/*
	 * If the number of threads is not a power of two then compute
	 * appropriate wrap-around value in the case of next slot counter
	 * overflow.
	 */
	if (count & (count - 1))
		lock->wrap = (UINT_MAX % count) + 1;
	else
		lock->wrap = 0;

	ck_pr_fence_store();
	return;
}

CK_CC_INLINE static void
ck_spinlock_anderson_lock(struct ck_spinlock_anderson *lock,
    struct ck_spinlock_anderson_thread **slot)
{
	unsigned int position, next;
	unsigned int count = lock->count;

	/*
	 * If count is not a power of 2, then it is possible for an overflow
	 * to reallocate beginning slots to more than one thread. To avoid this
	 * use a compare-and-swap.
	 */
	if (lock->wrap != 0) {
		position = ck_pr_load_uint(&lock->next);

		do {
			if (position == UINT_MAX)
				next = lock->wrap;
			else
				next = position + 1;
		} while (ck_pr_cas_uint_value(&lock->next, position,
					      next, &position) == false);

		position %= count;
	} else {
		position = ck_pr_faa_uint(&lock->next, 1);
		position &= lock->mask;
	}

	/* Serialize with respect to previous thread's store. */
	ck_pr_fence_load();

	/* Spin until slot is marked as unlocked. First slot is initialized to false. */
	while (ck_pr_load_uint(&lock->slots[position].locked) == true)
		ck_pr_stall();

	/* Prepare slot for potential re-use by another thread. */
	ck_pr_store_uint(&lock->slots[position].locked, true);
	ck_pr_fence_memory();

	*slot = lock->slots + position;
	return;
}

CK_CC_INLINE static void
ck_spinlock_anderson_unlock(struct ck_spinlock_anderson *lock,
    struct ck_spinlock_anderson_thread *slot)
{
	unsigned int position;

	ck_pr_fence_memory();

	/* Mark next slot as available. */
	if (lock->wrap == 0)
		position = (slot->position + 1) & lock->mask;
	else
		position = (slot->position + 1) % lock->count;

	ck_pr_store_uint(&lock->slots[position].locked, false);
	return;
}
#endif /* CK_F_SPINLOCK_ANDERSON */

#ifndef CK_F_SPINLOCK_FAS
#define CK_F_SPINLOCK_FAS

struct ck_spinlock_fas {
	unsigned int value;
};
typedef struct ck_spinlock_fas ck_spinlock_fas_t;

#define CK_SPINLOCK_FAS_INITIALIZER {false}

CK_CC_INLINE static void
ck_spinlock_fas_init(struct ck_spinlock_fas *lock)
{

	ck_pr_store_uint(&lock->value, false);
	return;
}

CK_CC_INLINE static bool
ck_spinlock_fas_trylock(struct ck_spinlock_fas *lock)
{
	bool value;

	value = ck_pr_fas_uint(&lock->value, true);
	if (value == false)
		ck_pr_fence_memory();

	return !value;
}

CK_CC_INLINE static bool
ck_spinlock_fas_locked(struct ck_spinlock_fas *lock)
{

	return ck_pr_load_uint(&lock->value);
}

CK_CC_INLINE static void
ck_spinlock_fas_lock(struct ck_spinlock_fas *lock)
{

	while (ck_pr_fas_uint(&lock->value, true) == true) {
		while (ck_pr_load_uint(&lock->value) == true)
			ck_pr_stall();
	}

	ck_pr_fence_memory();
	return;
}

CK_CC_INLINE static void
ck_spinlock_fas_lock_eb(struct ck_spinlock_fas *lock)
{
	ck_backoff_t backoff = CK_BACKOFF_INITIALIZER;

	while (ck_pr_fas_uint(&lock->value, true) == true)
		ck_backoff_eb(&backoff);

	ck_pr_fence_memory();
	return;
}

CK_CC_INLINE static void
ck_spinlock_fas_unlock(struct ck_spinlock_fas *lock)
{

	ck_pr_fence_memory();
	ck_pr_store_uint(&lock->value, false);
	return;
}
#endif /* CK_F_SPINLOCK_FAS */

#ifndef CK_F_SPINLOCK_CAS
#define CK_F_SPINLOCK_CAS
/*
 * This is a simple CACAS (TATAS) spinlock implementation.
 */
struct ck_spinlock_cas {
	unsigned int value;
};
typedef struct ck_spinlock_cas ck_spinlock_cas_t;

#define CK_SPINLOCK_CAS_INITIALIZER {false}

CK_CC_INLINE static void
ck_spinlock_cas_init(struct ck_spinlock_cas *lock)
{

	ck_pr_store_uint(&lock->value, false);
	return;
}

CK_CC_INLINE static bool
ck_spinlock_cas_trylock(struct ck_spinlock_cas *lock)
{
	unsigned int value;

	value = ck_pr_fas_uint(&lock->value, true);
	if (value == false)
		ck_pr_fence_memory();

	return !value;
}

CK_CC_INLINE static bool
ck_spinlock_cas_locked(struct ck_spinlock_cas *lock)
{

	return ck_pr_load_uint(&lock->value);
}

CK_CC_INLINE static void
ck_spinlock_cas_lock(struct ck_spinlock_cas *lock)
{

	while (ck_pr_cas_uint(&lock->value, false, true) == false) {
		while (ck_pr_load_uint(&lock->value) == true)
			ck_pr_stall();
	}

	ck_pr_fence_memory();
	return;
}

CK_CC_INLINE static void
ck_spinlock_cas_lock_eb(struct ck_spinlock_cas *lock)
{
	ck_backoff_t backoff = CK_BACKOFF_INITIALIZER;

	while (ck_pr_cas_uint(&lock->value, false, true) == false)
		ck_backoff_eb(&backoff);

	ck_pr_fence_memory();
	return;
}

CK_CC_INLINE static void
ck_spinlock_cas_unlock(struct ck_spinlock_cas *lock)
{

	/* Set lock state to unlocked. */
	ck_pr_fence_memory();
	ck_pr_store_uint(&lock->value, false);
	return;
}
#endif /* CK_F_SPINLOCK_CAS */

#ifndef CK_F_SPINLOCK_DEC
#define CK_F_SPINLOCK_DEC
/*
 * This is similar to the CACAS lock but makes use of an atomic decrement
 * operation to check if the lock value was decremented to 0 from 1. The
 * idea is that a decrement operation is cheaper than a compare-and-swap.
 */
struct ck_spinlock_dec {
	unsigned int value;
};
typedef struct ck_spinlock_dec ck_spinlock_dec_t;

#define CK_SPINLOCK_DEC_INITIALIZER {1}

CK_CC_INLINE static bool
ck_spinlock_dec_trylock(struct ck_spinlock_dec *lock)
{
	unsigned int value;

	value = ck_pr_fas_uint(&lock->value, 0);
	if (value == 1) {
		ck_pr_fence_memory();
		return true;
	}

	return false;
}

CK_CC_INLINE static bool
ck_spinlock_dec_locked(struct ck_spinlock_dec *lock)
{

	return ck_pr_load_uint(&lock->value) != 1;
}

CK_CC_INLINE static void
ck_spinlock_dec_lock(struct ck_spinlock_dec *lock)
{
	bool r;

	for (;;) {
		/*
		 * Only one thread is guaranteed to decrement lock to 0.
		 * Overflow must be protected against. No more than
		 * UINT_MAX lock requests can happen while the lock is held.
		 */
		ck_pr_dec_uint_zero(&lock->value, &r);
		ck_pr_fence_memory();
		if (r == true)
			break;

		/* Load value without generating write cycles. */
		while (ck_pr_load_uint(&lock->value) != 1)
			ck_pr_stall();
	}

	return;
}

CK_CC_INLINE static void
ck_spinlock_dec_lock_eb(struct ck_spinlock_dec *lock)
{
	ck_backoff_t backoff = CK_BACKOFF_INITIALIZER;
	bool r;

	for (;;) {
		ck_pr_dec_uint_zero(&lock->value, &r);
		if (r == true)
			break;

		ck_backoff_eb(&backoff);
	}

	ck_pr_fence_memory();
	return;
}

CK_CC_INLINE static void
ck_spinlock_dec_unlock(struct ck_spinlock_dec *lock)
{

	ck_pr_fence_memory();

	/* Unconditionally set lock value to 1 so someone can decrement lock to 0. */
	ck_pr_store_uint(&lock->value, 1);
	return;
}
#endif /* CK_F_SPINLOCK_DEC */

#ifndef CK_F_SPINLOCK_TICKET
#define CK_F_SPINLOCK_TICKET
/*
 * If 16-bit or 32-bit increment is supported, implement support for
 * trylock functionality on availability of 32-bit or 64-bit fetch-and-add
 * and compare-and-swap. This code path is only applied to x86*.
 */
#if defined(CK_MD_TSO) && (defined(__x86__) || defined(__x86_64__))
#if defined(CK_F_PR_FAA_32) && defined(CK_F_PR_INC_16) && defined(CK_F_PR_CAS_32)
#define CK_SPINLOCK_TICKET_TYPE         uint32_t
#define CK_SPINLOCK_TICKET_TYPE_BASE    uint16_t
#define CK_SPINLOCK_TICKET_INC(x)       ck_pr_inc_16(x)
#define CK_SPINLOCK_TICKET_CAS(x, y, z) ck_pr_cas_32(x, y, z)
#define CK_SPINLOCK_TICKET_FAA(x, y)    ck_pr_faa_32(x, y)
#define CK_SPINLOCK_TICKET_LOAD(x)      ck_pr_load_32(x)
#define CK_SPINLOCK_TICKET_INCREMENT    (0x00010000UL)
#define CK_SPINLOCK_TICKET_MASK         (0xFFFFUL)
#define CK_SPINLOCK_TICKET_SHIFT        (16)
#elif defined(CK_F_PR_FAA_64) && defined(CK_F_PR_INC_32) && defined(CK_F_PR_CAS_64)
#define CK_SPINLOCK_TICKET_TYPE         uint64_t
#define CK_SPINLOCK_TICKET_TYPE_BASE    uint32_t
#define CK_SPINLOCK_TICKET_INC(x)       ck_pr_inc_32(x)
#define CK_SPINLOCK_TICKET_CAS(x, y, z) ck_pr_cas_64(x, y, z)
#define CK_SPINLOCK_TICKET_FAA(x, y)    ck_pr_faa_64(x, y)
#define CK_SPINLOCK_TICKET_LOAD(x)      ck_pr_load_64(x)
#define CK_SPINLOCK_TICKET_INCREMENT    (0x0000000100000000ULL)
#define CK_SPINLOCK_TICKET_MASK         (0xFFFFFFFFULL)
#define CK_SPINLOCK_TICKET_SHIFT        (32)
#endif
#endif /* CK_MD_TSO */

#if defined(CK_SPINLOCK_TICKET_TYPE)
#define CK_F_SPINLOCK_TICKET_TRYLOCK

struct ck_spinlock_ticket {
	CK_SPINLOCK_TICKET_TYPE value;
};
typedef struct ck_spinlock_ticket ck_spinlock_ticket_t;
#define CK_SPINLOCK_TICKET_INITIALIZER { .value = 0 }

CK_CC_INLINE static void
ck_spinlock_ticket_init(struct ck_spinlock_ticket *ticket)
{

	ticket->value = 0;
	ck_pr_fence_store();
	return;
}

CK_CC_INLINE static void
ck_spinlock_ticket_lock(struct ck_spinlock_ticket *ticket)
{
	CK_SPINLOCK_TICKET_TYPE request, position;

	/* Get our ticket number and set next ticket number. */
	request = CK_SPINLOCK_TICKET_FAA(&ticket->value,
	    CK_SPINLOCK_TICKET_INCREMENT);

	position = request & CK_SPINLOCK_TICKET_MASK;
	request >>= CK_SPINLOCK_TICKET_SHIFT;

	while (request != position) {
		ck_pr_stall();
		position = CK_SPINLOCK_TICKET_LOAD(&ticket->value) &
		    CK_SPINLOCK_TICKET_MASK;
	}

	ck_pr_fence_memory();
	return;
}

CK_CC_INLINE static void
ck_spinlock_ticket_lock_pb(struct ck_spinlock_ticket *ticket, unsigned int c)
{
	CK_SPINLOCK_TICKET_TYPE request, position;
	ck_backoff_t backoff;

	/* Get our ticket number and set next ticket number. */
	request = CK_SPINLOCK_TICKET_FAA(&ticket->value,
	    CK_SPINLOCK_TICKET_INCREMENT);

	position = request & CK_SPINLOCK_TICKET_MASK;
	request >>= CK_SPINLOCK_TICKET_SHIFT;

	while (request != position) {
		ck_pr_stall();
		position = CK_SPINLOCK_TICKET_LOAD(&ticket->value) &
		    CK_SPINLOCK_TICKET_MASK;

		backoff = request - position;
		backoff <<= c;
		ck_backoff_eb(&backoff);
	}

	ck_pr_fence_memory();
	return;
}

CK_CC_INLINE static bool
ck_spinlock_ticket_trylock(struct ck_spinlock_ticket *ticket)
{
	CK_SPINLOCK_TICKET_TYPE snapshot, request, position;

	snapshot = CK_SPINLOCK_TICKET_LOAD(&ticket->value);
	position = snapshot & CK_SPINLOCK_TICKET_MASK;
	request = snapshot >> CK_SPINLOCK_TICKET_SHIFT;

	if (position != request)
		return false;

	if (CK_SPINLOCK_TICKET_CAS(&ticket->value,
	    snapshot, snapshot + CK_SPINLOCK_TICKET_INCREMENT) == false) {
		return false;
	}

	ck_pr_fence_memory();
	return true;
}

CK_CC_INLINE static void
ck_spinlock_ticket_unlock(struct ck_spinlock_ticket *ticket)
{

	ck_pr_fence_memory();
	CK_SPINLOCK_TICKET_INC((CK_SPINLOCK_TICKET_TYPE_BASE *)&ticket->value);
	return;
}

#undef CK_SPINLOCK_TICKET_TYPE
#undef CK_SPINLOCK_TICKET_TYPE_BASE
#undef CK_SPINLOCK_TICKET_INC
#undef CK_SPINLOCK_TICKET_FAA
#undef CK_SPINLOCK_TICKET_LOAD
#undef CK_SPINLOCK_TICKET_INCREMENT
#undef CK_SPINLOCK_TICKET_MASK
#undef CK_SPINLOCK_TICKET_SHIFT
#else
/*
 * MESI benefits from cacheline padding between next and current. This avoids
 * invalidation of current from the cache due to incoming lock requests.
 */
struct ck_spinlock_ticket {
	unsigned int next;
	unsigned int position;
};
typedef struct ck_spinlock_ticket ck_spinlock_ticket_t;

#define CK_SPINLOCK_TICKET_INITIALIZER {.next = 0, .position = 0}

CK_CC_INLINE static void
ck_spinlock_ticket_init(struct ck_spinlock_ticket *ticket)
{

	ticket->next = 0;
	ticket->position = 0;
	ck_pr_fence_store();

	return;
}

CK_CC_INLINE static void
ck_spinlock_ticket_lock(struct ck_spinlock_ticket *ticket)
{
	unsigned int request;

	/* Get our ticket number and set next ticket number. */
	request = ck_pr_faa_uint(&ticket->next, 1);

	/*
	 * Busy-wait until our ticket number is current.
	 * We can get away without a fence here assuming
	 * our position counter does not overflow.
	 */
	while (ck_pr_load_uint(&ticket->position) != request)
		ck_pr_stall();

	ck_pr_fence_memory();
	return;
}

CK_CC_INLINE static void
ck_spinlock_ticket_lock_pb(struct ck_spinlock_ticket *ticket, unsigned int c)
{
	ck_backoff_t backoff;
	unsigned int request, position;

	request = ck_pr_faa_uint(&ticket->next, 1);

	for (;;) {
		position = ck_pr_load_uint(&ticket->position);
		if (position == request)
			break;

		/* Overflow is handled fine, assuming 2s complement. */
		backoff = (request - position);
		backoff <<= c;

		/*
		 * Ideally, back-off from generating cache traffic for at least
		 * the amount of time necessary for the number of pending lock
		 * acquisition and relinquish operations (assuming an empty
		 * critical section).
		 */
		ck_backoff_eb(&backoff);
	}

	ck_pr_fence_memory();
	return;
}

CK_CC_INLINE static void
ck_spinlock_ticket_unlock(struct ck_spinlock_ticket *ticket)
{
	unsigned int update;

	ck_pr_fence_memory();

	/*
	 * Update current ticket value so next lock request can proceed.
	 * Overflow behavior is assumed to be roll-over, in which case,
	 * it is only an issue if there are 2^32 pending lock requests.
	 */
	update = ck_pr_load_uint(&ticket->position);
	ck_pr_store_uint(&ticket->position, update + 1);
	return;
}
#endif /* !CK_F_SPINLOCK_TICKET_TRYLOCK */
#endif /* CK_F_SPINLOCK_TICKET */

#ifndef CK_F_SPINLOCK_MCS
#define CK_F_SPINLOCK_MCS

struct ck_spinlock_mcs {
	unsigned int locked;
	struct ck_spinlock_mcs *next;
};
typedef struct ck_spinlock_mcs * ck_spinlock_mcs_t;
typedef struct ck_spinlock_mcs ck_spinlock_mcs_context_t;

#define CK_SPINLOCK_MCS_INITIALIZER         (NULL)
#define CK_SPINLOCK_MCS_CONTEXT_INITIALIZER {false, NULL}

CK_CC_INLINE static void
ck_spinlock_mcs_context_init(struct ck_spinlock_mcs *queue)
{

	ck_pr_store_uint(&queue->locked, false);
	ck_pr_store_ptr(&queue->next, NULL);
	return;
}

CK_CC_INLINE static bool
ck_spinlock_mcs_trylock(struct ck_spinlock_mcs **queue, struct ck_spinlock_mcs *node)
{

	node->locked = true;
	node->next = NULL;
	ck_pr_fence_store_atomic();

	if (ck_pr_cas_ptr(queue, NULL, node) == true) {
		ck_pr_fence_load();
		return true;
	}

	return false;
}

CK_CC_INLINE static bool
ck_spinlock_mcs_locked(struct ck_spinlock_mcs **queue)
{

	return ck_pr_load_ptr(queue) != NULL;
}

CK_CC_INLINE static void
ck_spinlock_mcs_lock(struct ck_spinlock_mcs **queue, struct ck_spinlock_mcs *node)
{
	struct ck_spinlock_mcs *previous;

	/*
	 * In the case that there is a successor, let them know they must wait
	 * for us to unlock.
	 */
	node->locked = true;
	node->next = NULL;
	ck_pr_fence_store_atomic();

	/*
	 * Swap current tail with current lock request. If the swap operation
	 * returns NULL, it means the queue was empty. If the queue was empty,
	 * then the operation is complete.
	 */
	previous = ck_pr_fas_ptr(queue, node);
	if (previous != NULL) {
		/* Let the previous lock holder know that we are waiting on them. */
		ck_pr_store_ptr(&previous->next, node);
		while (ck_pr_load_uint(&node->locked) == true)
			ck_pr_stall();
	}

	ck_pr_fence_load();
	return;
}

CK_CC_INLINE static void
ck_spinlock_mcs_unlock(struct ck_spinlock_mcs **queue, struct ck_spinlock_mcs *node)
{
	struct ck_spinlock_mcs *next;

	ck_pr_fence_memory();

	next = ck_pr_load_ptr(&node->next);
	if (next == NULL) {
		/*
		 * If there is no request following us then it is a possibilty
		 * that we are the current tail. In this case, we may just
		 * mark the spinlock queue as empty.
		 */
		if (ck_pr_load_ptr(queue) == node &&
		    ck_pr_cas_ptr(queue, node, NULL) == true) {
			return;
		}

		/*
		 * If the node is not the current tail then a lock operation is
		 * in-progress. In this case, busy-wait until the queue is in
		 * a consistent state to wake up the incoming lock request.
		 */
		for (;;) {
			next = ck_pr_load_ptr(&node->next);
			if (next != NULL)
				break;

			ck_pr_stall();
		}
	}

	/* Allow the next lock operation to complete. */
	ck_pr_store_uint(&next->locked, false);
	return;
}
#endif /* CK_F_SPINLOCK_MCS */

#ifndef CK_F_SPINLOCK_CLH
#define CK_F_SPINLOCK_CLH

struct ck_spinlock_clh {
	unsigned int wait;
	struct ck_spinlock_clh *previous;
};
typedef struct ck_spinlock_clh ck_spinlock_clh_t;

CK_CC_INLINE static void
ck_spinlock_clh_init(struct ck_spinlock_clh **lock, struct ck_spinlock_clh *unowned)
{

	ck_pr_store_ptr(&unowned->previous, NULL);
	ck_pr_store_uint(&unowned->wait, false);
	ck_pr_store_ptr(lock, unowned);
	ck_pr_fence_store();
	return;
}

CK_CC_INLINE static void
ck_spinlock_clh_lock(struct ck_spinlock_clh **queue, struct ck_spinlock_clh *thread)
{
	struct ck_spinlock_clh *previous;

	/* Indicate to the next thread on queue that they will have to block. */
	ck_pr_store_uint(&thread->wait, true);
	ck_pr_fence_store();

	/* Mark current request as last request. Save reference to previous request. */
	previous = ck_pr_fas_ptr(queue, thread);
	thread->previous = previous;

	/* Wait until previous thread is done with lock. */
	ck_pr_fence_load();
	while (ck_pr_load_uint(&previous->wait) == true)
		ck_pr_stall();

	return;
}

CK_CC_INLINE static void
ck_spinlock_clh_unlock(struct ck_spinlock_clh **thread)
{
	struct ck_spinlock_clh *previous;

	/*
	 * If there are waiters, they are spinning on the current node wait
	 * flag. The flag is cleared so that the successor may complete an
	 * acquisition. If the caller is pre-empted then the predecessor field
	 * may be updated by a successor's lock operation. In order to avoid
	 * this, save a copy of the predecessor before setting the flag.
	 */
	previous = thread[0]->previous;

	/* We have to pay this cost anyways, use it as a compiler barrier too. */
	ck_pr_fence_memory();
	ck_pr_store_uint(&(*thread)->wait, false);

	/*
	 * Predecessor is guaranteed not to be spinning on previous request,
	 * so update caller to use previous structure. This allows successor
	 * all the time in the world to successfully read updated wait flag.
	 */
	*thread = previous;
	return;
}
#endif /* CK_F_SPINLOCK_CLH */

#endif /* _CK_SPINLOCK_H */


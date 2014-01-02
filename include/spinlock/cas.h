/*
 * Copyright 2010-2014 Samy Al Bahra.
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

#ifndef _CK_SPINLOCK_CAS_H
#define _CK_SPINLOCK_CAS_H

#include <ck_backoff.h>
#include <ck_cc.h>
#include <ck_elide.h>
#include <ck_pr.h>
#include <stdbool.h>

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
		ck_pr_fence_acquire();

	return !value;
}

CK_CC_INLINE static bool
ck_spinlock_cas_locked(struct ck_spinlock_cas *lock)
{

	ck_pr_fence_load();
	return ck_pr_load_uint(&lock->value);
}

CK_CC_INLINE static void
ck_spinlock_cas_lock(struct ck_spinlock_cas *lock)
{

	while (ck_pr_cas_uint(&lock->value, false, true) == false) {
		while (ck_pr_load_uint(&lock->value) == true)
			ck_pr_stall();
	}

	ck_pr_fence_acquire();
	return;
}

CK_CC_INLINE static void
ck_spinlock_cas_lock_eb(struct ck_spinlock_cas *lock)
{
	ck_backoff_t backoff = CK_BACKOFF_INITIALIZER;

	while (ck_pr_cas_uint(&lock->value, false, true) == false)
		ck_backoff_eb(&backoff);

	ck_pr_fence_acquire();
	return;
}

CK_CC_INLINE static void
ck_spinlock_cas_unlock(struct ck_spinlock_cas *lock)
{

	/* Set lock state to unlocked. */
	ck_pr_fence_release();
	ck_pr_store_uint(&lock->value, false);
	return;
}

CK_ELIDE_PROTOTYPE(ck_spinlock_cas, ck_spinlock_cas_t,
    ck_spinlock_cas_locked, ck_spinlock_cas_lock,
    ck_spinlock_cas_locked, ck_spinlock_cas_unlock)

CK_ELIDE_TRYLOCK_PROTOTYPE(ck_spinlock_cas, ck_spinlock_cas_t,
    ck_spinlock_cas_locked, ck_spinlock_cas_trylock)

#endif /* CK_F_SPINLOCK_CAS */
#endif /* _CK_SPINLOCK_CAS_H */


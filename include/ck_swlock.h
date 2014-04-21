/*
 * Copyright 2014 Jaidev Sridhar.
 * Copyright 2014 Samy Al Bahra.
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

#ifndef _CK_SWLOCK_H
#define _CK_SWLOCK_H

#include <ck_elide.h>
#include <ck_limits.h>
#include <ck_pr.h>
#include <stdbool.h>
#include <stddef.h>

struct ck_swlock {
	uint32_t value;
};
typedef struct ck_swlock ck_swlock_t;

#define CK_SWLOCK_INITIALIZER {0}
#define CK_SWLOCK_WRITER_BIT (1UL << 31)
#define CK_SWLOCK_LATCH_BIT (1UL << 30)
#define CK_SWLOCK_READER_BITS (UINT32_MAX ^ (CK_SWLOCK_LATCH_BIT | CK_SWLOCK_WRITER_BIT))

CK_CC_INLINE static void
ck_swlock_init(struct ck_swlock *rw)
{

	rw->value = 0;
	ck_pr_barrier();
	return;
}

CK_CC_INLINE static void
ck_swlock_write_unlock(ck_swlock_t *rw)
{

	ck_pr_fence_release();
	ck_pr_and_32(&rw->value, CK_SWLOCK_READER_BITS);
	return;
}

CK_CC_INLINE static bool
ck_swlock_locked_writer(ck_swlock_t *rw)
{

	ck_pr_fence_load();
	return ck_pr_load_32(&rw->value) & CK_SWLOCK_WRITER_BIT;
}

CK_CC_INLINE static void
ck_swlock_write_downgrade(ck_swlock_t *rw)
{

	ck_pr_inc_32(&rw->value);
	ck_swlock_write_unlock(rw);
	return;
}

CK_CC_INLINE static bool
ck_swlock_locked(ck_swlock_t *rw)
{

	ck_pr_fence_load();
	return ck_pr_load_32(&rw->value);
}

CK_CC_INLINE static bool
ck_swlock_write_trylock(ck_swlock_t *rw)
{

	ck_pr_or_32(&rw->value, CK_SWLOCK_WRITER_BIT);

	if ((ck_pr_load_32(&rw->value) & CK_SWLOCK_READER_BITS) != 0) {
		ck_swlock_write_unlock(rw);
		return false;
	}

	ck_pr_fence_acquire();
	return true;
}

CK_ELIDE_TRYLOCK_PROTOTYPE(ck_swlock_write, ck_swlock_t,
    ck_swlock_locked, ck_swlock_write_trylock)

CK_CC_INLINE static void
ck_swlock_write_lock(ck_swlock_t *rw)
{

	ck_pr_or_32(&rw->value, CK_SWLOCK_WRITER_BIT);
	ck_pr_fence_acquire();

	while ((ck_pr_load_32(&rw->value) & CK_SWLOCK_READER_BITS) != 0)
		ck_pr_stall();

	return;
}

CK_CC_INLINE static void
ck_swlock_write_latch(ck_swlock_t *rw)
{

	ck_pr_or_32(&rw->value, CK_SWLOCK_WRITER_BIT);
	ck_pr_fence_acquire();

	/* Stall until readers have seen the latch and cleared. */
	while (ck_pr_cas_32(&rw->value, CK_SWLOCK_WRITER_BIT,
	    (CK_SWLOCK_WRITER_BIT | CK_SWLOCK_LATCH_BIT)) == false) {
		do {
			ck_pr_stall();
		} while (ck_pr_load_uint(&rw->value) != CK_SWLOCK_WRITER_BIT);
	}

	return;
}

CK_CC_INLINE static void
ck_swlock_write_unlatch(ck_swlock_t *rw)
{

	ck_pr_fence_release();
	ck_pr_store_32(&rw->value, 0);
	return;
}

CK_ELIDE_PROTOTYPE(ck_swlock_write, ck_swlock_t,
    ck_swlock_locked, ck_swlock_write_lock,
    ck_swlock_locked_writer, ck_swlock_write_unlock)

CK_CC_INLINE static bool
ck_swlock_read_trylock(ck_swlock_t *rw)
{

	if (ck_pr_faa_32(&rw->value, 1) & CK_SWLOCK_WRITER_BIT) {
		ck_pr_dec_32(&rw->value);
		return false;
	} 

	ck_pr_fence_acquire();
	return true;
}

CK_ELIDE_TRYLOCK_PROTOTYPE(ck_swlock_read, ck_swlock_t,
    ck_swlock_locked_writer, ck_swlock_read_trylock)

CK_CC_INLINE static void
ck_swlock_read_lock(ck_swlock_t *rw)
{
	uint32_t l;
	for (;;) {
		while (ck_pr_load_32(&rw->value) & CK_SWLOCK_WRITER_BIT)
			ck_pr_stall();

		l = ck_pr_faa_32(&rw->value, 1);

		ck_pr_fence_acquire();

		if (!(l & CK_SWLOCK_WRITER_BIT))
			return;
		
		ck_pr_dec_32(&rw->value);
	}

	return;
}

CK_CC_INLINE static bool
ck_swlock_read_trylatchlock(ck_swlock_t *rw)
{
	uint32_t l = ck_pr_load_32(&rw->value);

	if (l & CK_SWLOCK_WRITER_BIT)
		return false;

	l = ck_pr_faa_32(&rw->value, 1);
		
	ck_pr_fence_acquire();
	
	if (!(l & CK_SWLOCK_WRITER_BIT))
		return true;

	if (!(l & CK_SWLOCK_LATCH_BIT))
		ck_pr_dec_32(&rw->value);
	
	return false;
}


CK_CC_INLINE static void
ck_swlock_read_latchlock(ck_swlock_t *rw)
{
	uint32_t l;
	for (;;) {
		while (ck_pr_load_32(&rw->value) & CK_SWLOCK_WRITER_BIT)
			ck_pr_stall();

		l = ck_pr_faa_32(&rw->value, 1);

		ck_pr_fence_acquire();

		if (!(l & (CK_SWLOCK_LATCH_BIT | CK_SWLOCK_WRITER_BIT)))
			return;
		
		if (!(l & CK_SWLOCK_LATCH_BIT))
			ck_pr_dec_32(&rw->value);
	}

	return;
}


CK_CC_INLINE static bool
ck_swlock_locked_reader(ck_swlock_t *rw)
{

	ck_pr_fence_load();
	return ck_pr_load_32(&rw->value) & CK_SWLOCK_READER_BITS;
}

CK_CC_INLINE static void
ck_swlock_read_unlock(ck_swlock_t *rw)
{

	ck_pr_fence_release();
	ck_pr_dec_32(&rw->value);
	return;
}

CK_ELIDE_PROTOTYPE(ck_swlock_read, ck_swlock_t,
    ck_swlock_locked_writer, ck_swlock_read_lock,
    ck_swlock_locked_reader, ck_swlock_read_unlock)

/*
 * Recursive writer reader-writer lock implementation.
 */
struct ck_swlock_recursive {
	struct ck_swlock rw;
	uint32_t wc;
};
typedef struct ck_swlock_recursive ck_swlock_recursive_t;

#define CK_SWLOCK_RECURSIVE_INITIALIZER {CK_SWLOCK_INITIALIZER, 0}

CK_CC_INLINE static void
ck_swlock_recursive_write_lock(ck_swlock_recursive_t *rw)
{

	if (++rw->wc != 1) {
		return;
	}

	ck_swlock_write_lock(&rw->rw);
	return;
}

/* 
 * In recursive mode, latch must be the inner-most acquisition
 */

CK_CC_INLINE static void
ck_swlock_recursive_write_latch(ck_swlock_recursive_t *rw)
{

	ck_swlock_write_latch(&rw->rw);
	rw->wc++;
	return;
}

CK_CC_INLINE static bool
ck_swlock_recursive_write_trylock(ck_swlock_recursive_t *rw)
{

	if (ck_swlock_write_trylock(&rw->rw) == true) {
		rw->wc++;
		return true;
	}

	return false;
}

CK_CC_INLINE static void
ck_swlock_recursive_write_unlock(ck_swlock_recursive_t *rw)
{

	if (--rw->wc != 0)
		return;

	ck_swlock_write_unlock(&rw->rw);
	return;
}

CK_CC_INLINE static void
ck_swlock_recursive_write_unlatch(ck_swlock_recursive_t *rw)
{

	if (--rw->wc != 0) {
		ck_pr_store_32(&rw->rw.value, CK_SWLOCK_WRITER_BIT);
		return;
	}

	ck_pr_store_32(&rw->rw.value, 0);
	return;
}


CK_CC_INLINE static void
ck_swlock_recursive_read_lock(ck_swlock_recursive_t *rw)
{

	ck_swlock_read_lock(&rw->rw);
	return;
}

CK_CC_INLINE static void
ck_swlock_recursive_read_latchlock(ck_swlock_recursive_t *rw)
{

	ck_swlock_read_latchlock(&rw->rw);
	return;
}

CK_CC_INLINE static bool
ck_swlock_recursive_read_trylock(ck_swlock_recursive_t *rw)
{

	return ck_swlock_read_trylock(&rw->rw);
}

CK_CC_INLINE static void
ck_swlock_recursive_read_unlock(ck_swlock_recursive_t *rw)
{

	ck_swlock_read_unlock(&rw->rw);
	return;
}

#endif /* _CK_SWLOCK_H */


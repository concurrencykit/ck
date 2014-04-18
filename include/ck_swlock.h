/*
 * Copyright 2011-2014 Samy Al Bahra.
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
#include <ck_pr.h>
#include <stdbool.h>
#include <stddef.h>

struct ck_swlock {
	uint32_t writer;
	uint32_t n_readers;
};
typedef struct ck_swlock ck_swlock_t;

#define CK_SWLOCK_INITIALIZER {0, 0}
#define CK_SWLOCK_LATCH_BIT (1 << 31)
#define CK_SWLOCK_READER_BITS (UINT32_MAX ^ CK_SWLOCK_LATCH_BIT)

CK_CC_INLINE static void
ck_swlock_init(struct ck_swlock *rw)
{

	rw->writer = 0;
	rw->n_readers = 0;
	ck_pr_fence_store();
	return;
}

CK_CC_INLINE static void
ck_swlock_write_unlock(ck_swlock_t *rw)
{

	ck_pr_store_32(&rw->writer, 0);
	return;
}

CK_CC_INLINE static bool
ck_swlock_locked_writer(ck_swlock_t *rw)
{

	return ck_pr_load_32(&rw->writer);
}

CK_CC_INLINE static void
ck_swlock_write_downgrade(ck_swlock_t *rw)
{

	ck_pr_inc_32(&rw->n_readers);
	ck_swlock_write_unlock(rw);
	return;
}

CK_CC_INLINE static bool
ck_swlock_locked(ck_swlock_t *rw)
{
	uint32_t r;

	r = ck_pr_load_32(&rw->writer);

	return ck_pr_load_32(&rw->n_readers) | r;
}

CK_CC_INLINE static bool
ck_swlock_write_trylock(ck_swlock_t *rw)
{

	ck_pr_store_32(&rw->writer, 1);

	ck_pr_fence_atomic_load();

	if (ck_pr_load_32(&rw->n_readers) != 0) {
		ck_swlock_write_unlock(rw);
		return false;
	}

	return true;
}

CK_ELIDE_TRYLOCK_PROTOTYPE(ck_swlock_write, ck_swlock_t,
    ck_swlock_locked, ck_swlock_write_trylock)

CK_CC_INLINE static void
ck_swlock_write_lock(ck_swlock_t *rw)
{

	ck_pr_store_32(&rw->writer, 1);
	
	ck_pr_fence_atomic_load();

	while (ck_pr_load_32(&rw->n_readers) != 0)
		ck_pr_stall();

	return;
}

CK_CC_INLINE static void
ck_swlock_write_latch(ck_swlock_t *rw)
{

	ck_pr_store_32(&rw->writer, 1);
	
	ck_pr_fence_atomic_load();
	
	while (ck_pr_cas_32(&rw->n_readers, 0, CK_SWLOCK_LATCH_BIT) == false) {
		/* Stall until readers have seen the latch and cleared */
		ck_pr_stall();
	}

	return;
}

CK_CC_INLINE static void
ck_swlock_write_unlatch(ck_swlock_t *rw)
{

	ck_pr_store_32(&rw->n_readers, 0);
	
	ck_swlock_write_unlock(rw);
	
	return;
}

CK_ELIDE_PROTOTYPE(ck_swlock_write, ck_swlock_t,
    ck_swlock_locked, ck_swlock_write_lock,
    ck_swlock_locked_writer, ck_swlock_write_unlock)

CK_CC_INLINE static bool
ck_swlock_read_trylock(ck_swlock_t *rw)
{

	if (ck_pr_load_32(&rw->writer) != 0)
		return false;

	if (ck_pr_faa_32(&rw->n_readers, 1) & CK_SWLOCK_LATCH_BIT) {
		return false;
	}

	/*
	 * Serialize with respect to concurrent write
	 * lock operation.
	 */
	ck_pr_fence_atomic_load();

	if (ck_pr_load_32(&rw->writer) == 0) {
		ck_pr_fence_load();
		return true;
	}

	ck_pr_dec_32(&rw->n_readers);
	return false;
}

CK_ELIDE_TRYLOCK_PROTOTYPE(ck_swlock_read, ck_swlock_t,
    ck_swlock_locked_writer, ck_swlock_read_trylock)

CK_CC_INLINE static void
ck_swlock_read_lock(ck_swlock_t *rw)
{

	for (;;) {
		while (ck_pr_load_32(&rw->writer) != 0)
			ck_pr_stall();

		ck_pr_inc_32(&rw->n_readers);

		/*
		 * Serialize with respect to concurrent write
		 * lock operation.
		 */
		ck_pr_fence_atomic_load();

		if (ck_pr_load_32(&rw->writer) == 0)
			break;

		ck_pr_dec_32(&rw->n_readers);
	}

	/* Acquire semantics are necessary. */
	ck_pr_fence_load();
	return;
}

CK_CC_INLINE static void
ck_swlock_read_latchlock(ck_swlock_t *rw)
{

	for (;;) {

		while (ck_pr_load_32(&rw->writer) != 0)
			ck_pr_stall();

		if (ck_pr_faa_32(&rw->n_readers, 1) & CK_SWLOCK_LATCH_BIT) {
			/* Writer has latched, stall the reader */
			continue;
		}

		/*
		 * Serialize with respect to concurrent write
		 * lock operation.
		 */
		ck_pr_fence_atomic_load();

		if (ck_pr_load_32(&rw->writer) == 0)
			break;
		
		ck_pr_dec_32(&rw->n_readers);
	}

	/* Acquire semantics are necessary. */
	ck_pr_fence_load();
	return;
}


CK_CC_INLINE static bool
ck_swlock_locked_reader(ck_swlock_t *rw)
{

	ck_pr_fence_load();
	return (ck_pr_load_32(&rw->n_readers) & CK_SWLOCK_READER_BITS);
}

CK_CC_INLINE static void
ck_swlock_read_unlock(ck_swlock_t *rw)
{

	ck_pr_fence_load_atomic();
	ck_pr_dec_32(&rw->n_readers);
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

	ck_pr_store_32(&rw->rw.writer, 1);

	ck_pr_fence_atomic_load();

	while (ck_pr_load_32(&rw->rw.n_readers) != 0)
		ck_pr_stall();

	rw->wc++;
	return;
}

CK_CC_INLINE static void
ck_swlock_recursive_write_latch(ck_swlock_recursive_t *rw)
{
	ck_pr_store_32(&rw->rw.writer, 1);

	ck_pr_fence_atomic_load();

	while (ck_pr_cas_32(&rw->rw.n_readers, 0, CK_SWLOCK_LATCH_BIT) == false)
		ck_pr_stall();

	rw->wc++;
	return;
}

CK_CC_INLINE static bool
ck_swlock_recursive_write_trylock(ck_swlock_recursive_t *rw)
{

	ck_pr_store_32(&rw->rw.writer, 1);

	ck_pr_fence_atomic_load();

	if (ck_pr_load_32(&rw->rw.n_readers) != 0) {
		ck_pr_store_32(&rw->rw.writer, 0);
		return false;
	}

	rw->wc++;
	return true;
}

CK_CC_INLINE static void
ck_swlock_recursive_write_unlock(ck_swlock_recursive_t *rw)
{

	if (--rw->wc == 0) {
		ck_pr_fence_release();
		ck_pr_store_32(&rw->rw.writer, 0);
	}

	return;
}

CK_CC_INLINE static void
ck_swlock_recursive_write_unlatch(ck_swlock_recursive_t *rw)
{
	ck_pr_store_32(&rw->rw.n_readers, 0);

	ck_swlock_recursive_write_unlock(rw);

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


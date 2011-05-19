/*
 * Copyright 2011 Samy Al Bahra.
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

#ifndef _CK_RWLOCK_H
#define _CK_RWLOCK_H

#include <ck_pr.h>
#include <stdbool.h>
#include <stddef.h>

struct ck_rwlock {
	unsigned int writer;
	unsigned int n_readers;
};
typedef struct ck_rwlock ck_rwlock_t;

#define CK_RWLOCK_INITIALIZER {false, 0}

CK_CC_INLINE static void
ck_rwlock_init(struct ck_rwlock *rw)
{

	rw->writer = 0;
	rw->n_readers = 0;
	ck_pr_fence_memory();
	return;
}

CK_CC_INLINE static void
ck_rwlock_write_lock(ck_rwlock_t *rw)
{

	while (ck_pr_fas_uint(&rw->writer, true) == true)
		ck_pr_stall();

	while (ck_pr_load_uint(&rw->n_readers) != 0)
		ck_pr_stall();

	return;
}

CK_CC_INLINE static void
ck_rwlock_write_unlock(ck_rwlock_t *rw)
{

	ck_pr_store_uint(&rw->writer, false);
	return;
}

CK_CC_INLINE static void
ck_rwlock_read_lock(ck_rwlock_t *rw)
{

	for (;;) {
		while (ck_pr_load_uint(&rw->writer) == true)
			ck_pr_stall();

		ck_pr_inc_uint(&rw->n_readers);
		if (ck_pr_load_uint(&rw->writer) == false)
			break;
		ck_pr_dec_uint(&rw->n_readers);
	}

	return;
}

CK_CC_INLINE static void
ck_rwlock_read_unlock(ck_rwlock_t *rw)
{

	ck_pr_dec_uint(&rw->n_readers);
	return;
}

#endif /* _CK_RWLOCK_H */

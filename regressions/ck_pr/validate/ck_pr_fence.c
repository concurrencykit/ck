/*
 * Copyright 2009-2018 Samy Al Bahra.
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

#include <ck_pr.h>
#include "../../common.h"

int
main(void)
{
	int r = 0;

	/* Below serves as a marker. */
	ck_pr_sub_int(&r, 31337);

	/*
	 * This is a simple test to help ensure all fences compile or crash
	 * on target. Below are generated according to the underlying memory
	 * model's ordering.
	 */
	ck_pr_fence_atomic();
	ck_pr_fence_atomic_store();
	ck_pr_fence_atomic_load();
	ck_pr_fence_store_atomic();
	ck_pr_fence_load_atomic();
	ck_pr_fence_load();
	ck_pr_fence_load_store();
	ck_pr_fence_store();
	ck_pr_fence_store_load();
	ck_pr_fence_memory();
	ck_pr_fence_release();
	ck_pr_fence_acquire();
	ck_pr_fence_acqrel();
	ck_pr_fence_lock();
	ck_pr_fence_unlock();

	/* Below serves as a marker. */
	ck_pr_sub_int(&r, 31337);

	/* The following are generating assuming RMO. */
	ck_pr_fence_strict_atomic();
	ck_pr_fence_strict_atomic_store();
	ck_pr_fence_strict_atomic_load();
	ck_pr_fence_strict_store_atomic();
	ck_pr_fence_strict_load_atomic();
	ck_pr_fence_strict_load();
	ck_pr_fence_strict_load_store();
	ck_pr_fence_strict_store();
	ck_pr_fence_strict_store_load();
	ck_pr_fence_strict_memory();
	ck_pr_fence_strict_release();
	ck_pr_fence_strict_acquire();
	ck_pr_fence_strict_acqrel();
	ck_pr_fence_strict_lock();
	ck_pr_fence_strict_unlock();
	return 0;
}


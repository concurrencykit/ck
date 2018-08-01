/*
 * Copyright 2013-2015 Samy Al Bahra.
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

#ifndef CK_PR_PPC64_RTM_H
#define CK_PR_PPC64_RTM_H

#ifndef CK_PR_PPC64_H
#error Do not include this file directly, use ck_pr.h
#endif

#define CK_F_PR_RTM

#include <htmxintrin.h>

#include <ck_cc.h>
#include <ck_stdbool.h>

#define CK_PR_RTM_STARTED	(~0U)
#define CK_PR_RTM_EXPLICIT	(1 << 0)
#define CK_PR_RTM_RETRY		(1 << 1)
#define CK_PR_RTM_CONFLICT	(1 << 2)
#define CK_PR_RTM_CAPACITY	(1 << 3)
#define CK_PR_RTM_DEBUG		(1 << 4)
#define CK_PR_RTM_NESTED	(1 << 5)
#define CK_PR_RTM_CODE(x)	(((x) >> 24) & 0xFF)

CK_CC_INLINE static unsigned int
ck_pr_rtm_begin(void)
{
	if (__TM_simplebegin())
		return CK_PR_RTM_STARTED;
	return CK_PR_RTM_RETRY;
}

CK_CC_INLINE static void
ck_pr_rtm_end(void)
{
	__TM_end();
	return;
}

CK_CC_INLINE static void
ck_pr_rtm_abort(const unsigned int unused CK_CC_UNUSED)
{
	__TM_abort();
	return;
}

#endif /* CK_PR_PPC64_RTM_H */

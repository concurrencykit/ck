/*
 * Copyright 2011-2012 Samy Al Bahra.
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

#include <ck_cc.h>
#include <ck_pr.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef __linux__
#include <sched.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#elif defined(__MACH__)
#include <mach/mach.h>
#include <mach/thread_policy.h>
#endif

#ifndef CORES
#define CORES 8
#endif

struct affinity {
	unsigned int delta;
	unsigned int request;
};

#define AFFINITY_INITIALIZER {0, 0}

#ifdef __linux__
#ifndef gettid
static pid_t
gettid(void)
{
	return syscall(__NR_gettid);
}
#endif /* gettid */

CK_CC_UNUSED static int
aff_iterate(struct affinity *acb)
{
	cpu_set_t s;
	unsigned int c;

	c = ck_pr_faa_uint(&acb->request, acb->delta);
	CPU_ZERO(&s);
	CPU_SET(c % CORES, &s);

	return sched_setaffinity(gettid(), sizeof(s), &s);
}
#elif defined(__MACH__)
CK_CC_UNUSED static int
aff_iterate(struct affinity *acb)
{
	thread_affinity_policy_data_t policy;
	unsigned int c;

	c = ck_pr_faa_uint(&acb->request, acb->delta) % CORES;
	policy.affinity_tag = c;
	return thread_policy_set(mach_thread_self(),
				 THREAD_AFFINITY_POLICY,
				 (thread_policy_t)&policy,
				 THREAD_AFFINITY_POLICY_COUNT);
}
#else
CK_CC_UNUSED static int
aff_iterate(struct affinity *acb CK_CC_UNUSED)
{

	return (0);
}
#endif

CK_CC_INLINE static uint64_t
rdtsc(void)
{
#if defined(__x86_64__)
	uint32_t eax = 0, edx;
#if defined(CK_MD_RDTSCP)
	__asm__ __volatile__("rdtscp"
				: "+a" (eax), "=d" (edx)
				:
				: "%ecx", "memory");

	return (((uint64_t)edx << 32) | eax);
#else

        __asm__ __volatile__("cpuid;"
                             "rdtsc;"
                                : "+a" (eax), "=d" (edx)
                                :
                                : "%ecx", "%ebx", "memory");

        __asm__ __volatile__("xorl %%eax, %%eax;"
                             "cpuid;"
                                :
                                :
                                : "%eax", "%ebx", "%ecx", "%edx", "memory");

        return (((uint64_t)edx << 32) | eax);
#endif /* !CK_MD_RDTSCP */
#elif defined(__sparcv9__)
	uint64_t r;

        __asm__ __volatile__("rd %%tick, %0"
				: "=r" (r)
				:
				: "memory");
        return r;
#elif defined(__ppc64__)
	uint32_t high, low, snapshot;

	do {
	  __asm__ __volatile__("isync;"
			       "mftbu %0;"
			       "mftb  %1;"
			       "mftbu %2;"
				: "=r" (high), "=r" (low), "=r" (snapshot)
				:
				: "memory");
	} while (snapshot != high);

	return (((uint64_t)high << 32) | low);
#else
	return 0;
#endif
}

CK_CC_USED static void
ck_error(const char *message, ...)
{
	va_list ap;

	va_start(ap, message);
	vfprintf(stderr, message, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}


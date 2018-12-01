/*
 * Copyright 2018 Paul Khuong.
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

#include <assert.h>
#include <ck_cc.h>
#include <ck_ec.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "../../common.h"

#ifndef STEPS
#define STEPS (65536 * 64)
#endif

static int gettime(const struct ck_ec_ops *, struct timespec *out);
static void wake32(const struct ck_ec_ops *, const uint32_t *);
static void wait32(const struct ck_ec_wait_state *,
		   const uint32_t *, uint32_t, const struct timespec *);
static void wake64(const struct ck_ec_ops *, const uint64_t *);
static void wait64(const struct ck_ec_wait_state *,
		   const uint64_t *, uint64_t, const struct timespec *);

static const struct ck_ec_ops test_ops = {
	.gettime = gettime,
	.wait32 = wait32,
	.wait64 = wait64,
	.wake32 = wake32,
	.wake64 = wake64
};

#ifndef __linux__
static int gettime(const struct ck_ec_ops *ops, struct timespec *out)
{
	(void)out;

	assert(ops == &test_ops);
	return -1;
}

static void wait32(const struct ck_ec_wait_state *state,
		   const uint32_t *address, uint32_t expected,
		   const struct timespec *deadline)
{
	(void)address;
	(void)expected;
	(void)deadline;

	assert(state->ops == &test_ops);
	return;
}

static void wait64(const struct ck_ec_wait_state *state,
		   const uint64_t *address, uint64_t expected,
		   const struct timespec *deadline)
{
	(void)address;
	(void)expected;
	(void)deadline;

	assert(state->ops == &test_ops);
	return;
}

static void wake32(const struct ck_ec_ops *ops, const uint32_t *address)
{
	(void)address;

	assert(ops == &test_ops);
	return;
}

static void wake64(const struct ck_ec_ops *ops, const uint64_t *address)
{
	(void)address;

	assert(ops == &test_ops);
	return;
}
#else
#include <linux/futex.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

static int gettime(const struct ck_ec_ops *ops, struct timespec *out)
{
	assert(ops == &test_ops);
	return clock_gettime(CLOCK_MONOTONIC, out);
}

static void wait32(const struct ck_ec_wait_state *state,
		   const uint32_t *address, uint32_t expected,
		   const struct timespec *deadline)
{
	assert(state->ops == &test_ops);
	syscall(SYS_futex, address,
		FUTEX_WAIT_BITSET, expected, deadline,
		NULL, deadline, 0);
	return;
}

static void wait64(const struct ck_ec_wait_state *state,
		   const uint64_t *address, uint64_t expected,
		   const struct timespec *deadline)
{
	const void *low_half;

	assert(state->ops == &test_ops);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	low_half = address;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	low_half = (uintptr_t)address + sizeof(uint32_t);
#else
# error "__BYTE_ORDER__ must be defined."
#endif

	syscall(SYS_futex, low_half,
		FUTEX_WAIT_BITSET, (uint32_t)expected, deadline,
		NULL, deadline, 0);
	return;
}

static void wake32(const struct ck_ec_ops *ops, const uint32_t *address)
{
	assert(ops == &test_ops);
	syscall(SYS_futex, address,
		FUTEX_WAKE, INT_MAX,
		/* ignored arguments */NULL, NULL, 0);
	return;
}

static void wake64(const struct ck_ec_ops *ops, const uint64_t *address)
{
	const void *low_half;

	assert(ops == &test_ops);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	low_half = address;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	low_half = (uintptr_t)address + sizeof(uint32_t);
#else
# error "__BYTE_ORDER__ must be defined."
#endif

	syscall(SYS_futex, low_half,
		FUTEX_WAKE, INT_MAX,
		/* ignored arguments */NULL, NULL, 0);
	return;
}
#endif /* __linux__ */

static const struct ck_ec_mode sp = {
	.ops = &test_ops,
	.single_producer = true
};

static const struct ck_ec_mode mp = {
	.ops = &test_ops,
	.single_producer = false
};

static CK_CC_FORCE_INLINE void bench32(const struct ck_ec_mode mode)
{
	ck_ec32_t ec CK_CC_CACHELINE = CK_EC_INITIALIZER;
	uint64_t a;
	uint64_t baseline = 1000 * 1000;
	uint32_t value;

	for (size_t i = 0; i < STEPS; i++) {
		uint64_t s = rdtsc();
		uint64_t elapsed = rdtsc() - s;

		if (elapsed < baseline) {
			baseline = elapsed;
		}
	}

	/* Read value. */
	a = 0;
	value = 0;
	for (size_t i = 0; i < STEPS / 4; i++) {
		uint64_t s = rdtsc();

		value ^= ck_ec32_value(&ec);
		value ^= ck_ec32_value(&ec);
		value ^= ck_ec32_value(&ec);
		value ^= ck_ec32_value(&ec);

		__asm__ volatile("" :: "r"(value));
		a += rdtsc() - s - baseline;
	}

	printf("%s ec32_value: %" PRIu64 "\n",
	       (mode.single_producer ? "SP" : "MP"), a / STEPS);

	/* Wait (fast path). */
	a = 0;
	for (size_t i = 0; i < STEPS / 4; i++) {
		uint64_t s = rdtsc();

		ck_ec32_wait(&ec, &mode, 1, NULL);
		ck_ec32_wait(&ec, &mode, 1, NULL);
		ck_ec32_wait(&ec, &mode, 1, NULL);
		ck_ec32_wait(&ec, &mode, 1, NULL);

		a += rdtsc() - s - baseline;
	}

	printf("%s ec32_wait fast: %" PRIu64 "\n",
	       (mode.single_producer ? "SP" : "MP"), a / STEPS);

	/* trywait. */
	a = 0;
	for (size_t i = 0; i < STEPS / 4; i++) {
		struct timespec past = { .tv_sec = 0 };
		uint64_t s = rdtsc();

		ck_ec32_wait(&ec, &mode, 0, &past);
		ck_ec32_wait(&ec, &mode, 0, &past);
		ck_ec32_wait(&ec, &mode, 0, &past);
		ck_ec32_wait(&ec, &mode, 0, &past);

		a += rdtsc() - s - baseline;
	}

	printf("%s ec32_wait timeout: %" PRIu64 "\n",
	       (mode.single_producer ? "SP" : "MP"), a / STEPS);

	/* Inc (no waiter). */
	assert(!ck_ec32_has_waiters(&ec));
	a = 0;
	for (size_t i = 0; i < STEPS / 4; i++) {
		uint64_t s = rdtsc();

		ck_ec32_inc(&ec, &mode);
		ck_ec32_inc(&ec, &mode);
		ck_ec32_inc(&ec, &mode);
		ck_ec32_inc(&ec, &mode);

		a += rdtsc() - s - baseline;
	}

	printf("%s ec32_inc: %" PRIu64 "\n",
	       (mode.single_producer ? "SP" : "MP"), a / STEPS);

	/* Inc (with waiter). */
	assert(!ck_ec32_has_waiters(&ec));
	a = 0;
	for (size_t i = 0; i < STEPS; i++) {
		struct timespec past = { .tv_sec = 1 };
		uint64_t s;

		ck_ec32_wait(&ec, &mode, ck_ec32_value(&ec), &past);
		assert(ck_ec32_has_waiters(&ec));

		s = rdtsc();
		ck_ec32_inc(&ec, &mode);
		a += rdtsc() - s - baseline;
	}

	printf("%s ec32_inc slow: %" PRIu64 "\n",
	       (mode.single_producer ? "SP" : "MP"), a / STEPS);

	/* Add (no waiter). */
	assert(!ck_ec32_has_waiters(&ec));
	a = 0;
	for (size_t i = 0; i < STEPS / 4; i++) {
		uint64_t s = rdtsc();

		ck_ec32_add(&ec, &mode, i + 1);
		ck_ec32_add(&ec, &mode, i + 2);
		ck_ec32_add(&ec, &mode, i + 3);
		ck_ec32_add(&ec, &mode, i + 4);

		a += rdtsc() - s - baseline;
	}

	printf("%s ec32_add: %" PRIu64 "\n",
	       (mode.single_producer ? "SP" : "MP"), a / STEPS);

	assert(!ck_ec32_has_waiters(&ec));
	a = 0;
	for (size_t i = 0; i < STEPS; i++) {
		struct timespec past = { .tv_sec = 1 };
		uint64_t s;

		ck_ec32_wait(&ec, &mode, ck_ec32_value(&ec), &past);
		assert(ck_ec32_has_waiters(&ec));

		s = rdtsc();
		ck_ec32_add(&ec, &mode, i + 1);
		a += rdtsc() - s - baseline;
	}

	printf("%s ec32_add slow: %" PRIu64 "\n",
	       (mode.single_producer ? "SP" : "MP"), a / STEPS);
	return;
}

#ifdef CK_F_EC64
static CK_CC_FORCE_INLINE void bench64(const struct ck_ec_mode mode)
{
	ck_ec64_t ec CK_CC_CACHELINE = CK_EC_INITIALIZER;
	uint64_t a;
	uint64_t baseline = 1000 * 1000;
	uint64_t value;

	for (size_t i = 0; i < STEPS; i++) {
		uint64_t s = rdtsc();
		uint64_t elapsed = rdtsc() - s;

		if (elapsed < baseline) {
			baseline = elapsed;
		}
	}

	/* Read value. */
	a = 0;
	value = 0;
	for (size_t i = 0; i < STEPS / 4; i++) {
		uint64_t s = rdtsc();

		value ^= ck_ec64_value(&ec);
		value ^= ck_ec64_value(&ec);
		value ^= ck_ec64_value(&ec);
		value ^= ck_ec64_value(&ec);

		__asm__ volatile("" :: "r"(value));
		a += rdtsc() - s - baseline;
	}

	printf("%s ec64_value: %" PRIu64 "\n",
	       (mode.single_producer ? "SP" : "MP"), a / STEPS);

	/* Wait (fast path). */
	a = 0;
	for (size_t i = 0; i < STEPS / 4; i++) {
		uint64_t s = rdtsc();

		ck_ec64_wait(&ec, &mode, 1, NULL);
		ck_ec64_wait(&ec, &mode, 1, NULL);
		ck_ec64_wait(&ec, &mode, 1, NULL);
		ck_ec64_wait(&ec, &mode, 1, NULL);

		a += rdtsc() - s - baseline;
	}

	printf("%s ec64_wait fast: %" PRIu64 "\n",
	       (mode.single_producer ? "SP" : "MP"), a / STEPS);

	/* trywait. */
	a = 0;
	for (size_t i = 0; i < STEPS / 4; i++) {
		struct timespec past = { .tv_sec = 0 };
		uint64_t s = rdtsc();

		ck_ec64_wait(&ec, &mode, 0, &past);
		ck_ec64_wait(&ec, &mode, 0, &past);
		ck_ec64_wait(&ec, &mode, 0, &past);
		ck_ec64_wait(&ec, &mode, 0, &past);

		a += rdtsc() - s - baseline;
	}

	printf("%s ec64_wait timeout: %" PRIu64 "\n",
	       (mode.single_producer ? "SP" : "MP"), a / STEPS);

	/* Inc (no waiter). */
	assert(!ck_ec64_has_waiters(&ec));
	a = 0;
	for (size_t i = 0; i < STEPS / 4; i++) {
		uint64_t s = rdtsc();

		ck_ec64_inc(&ec, &mode);
		ck_ec64_inc(&ec, &mode);
		ck_ec64_inc(&ec, &mode);
		ck_ec64_inc(&ec, &mode);

		a += rdtsc() - s - baseline;
	}

	printf("%s ec64_inc: %" PRIu64 "\n",
	       (mode.single_producer ? "SP" : "MP"), a / STEPS);

	/* Inc (with waiter). */
	assert(!ck_ec64_has_waiters(&ec));
	a = 0;
	for (size_t i = 0; i < STEPS; i++) {
		struct timespec past = { .tv_sec = 1 };
		uint64_t s;

		ck_ec64_wait(&ec, &mode, ck_ec64_value(&ec), &past);
		assert(ck_ec64_has_waiters(&ec));

		s = rdtsc();
		ck_ec64_inc(&ec, &mode);
		a += rdtsc() - s - baseline;
	}

	printf("%s ec64_inc slow: %" PRIu64 "\n",
	       (mode.single_producer ? "SP" : "MP"), a / STEPS);

	/* Add (no waiter). */
	assert(!ck_ec64_has_waiters(&ec));
	a = 0;
	for (size_t i = 0; i < STEPS / 4; i++) {
		uint64_t s = rdtsc();

		ck_ec64_add(&ec, &mode, i + 1);
		ck_ec64_add(&ec, &mode, i + 2);
		ck_ec64_add(&ec, &mode, i + 3);
		ck_ec64_add(&ec, &mode, i + 4);

		a += rdtsc() - s - baseline;
	}

	printf("%s ec64_add: %" PRIu64 "\n",
	       (mode.single_producer ? "SP" : "MP"), a / STEPS);

	assert(!ck_ec64_has_waiters(&ec));
	a = 0;
	for (size_t i = 0; i < STEPS; i++) {
		struct timespec past = { .tv_sec = 1 };
		uint64_t s;

		ck_ec64_wait(&ec, &mode, ck_ec64_value(&ec), &past);
		assert(ck_ec64_has_waiters(&ec));

		s = rdtsc();
		ck_ec64_add(&ec, &mode, i + 1);
		a += rdtsc() - s - baseline;
	}

	printf("%s ec64_add slow: %" PRIu64 "\n",
	       (mode.single_producer ? "SP" : "MP"), a / STEPS);
	return;
}
#endif /* CK_F_EC64 */

int
main(void)
{
	printf("SP ec32\n");
	bench32(sp);
	printf("\nMP ec32\n");
	bench32(mp);

#ifdef CK_F_EC64
	printf("\nSP ec64\n");
	bench64(sp);
	printf("\nMP ec64\n");
	bench64(mp);
#endif /* CK_F_EC64 */

	return 0;
}

#include <assert.h>
#include <ck_ec.h>
#include <ck_stdbool.h>

#include "fuzz_harness.h"

static int gettime(const struct ck_ec_ops *, struct timespec *out);
static void wake32(const struct ck_ec_ops *, const uint32_t *);
static void wait32(const struct ck_ec_wait_state *, const uint32_t *,
		   uint32_t, const struct timespec *);
static void wake64(const struct ck_ec_ops *, const uint64_t *);
static void wait64(const struct ck_ec_wait_state *, const uint64_t *,
		   uint64_t, const struct timespec *);

static const struct ck_ec_ops test_ops = {
	.gettime = gettime,
	.wait32 = wait32,
	.wait64 = wait64,
	.wake32 = wake32,
	.wake64 = wake64
};

static const struct ck_ec_mode modes[] = {
	{
		.single_producer = true,
		.ops = &test_ops
	},
	{
		.single_producer = false,
		.ops = &test_ops
	},
};

static bool woken = false;

static int gettime(const struct ck_ec_ops *ops, struct timespec *out)
{
	(void)out;

	assert(ops == &test_ops);
	return -1;
}

static void wait32(const struct ck_ec_wait_state *state, const uint32_t *addr,
		   uint32_t expected, const struct timespec *deadline)
{
	(void)addr;
	(void)expected;
	(void)deadline;

	assert(state->ops == &test_ops);
	return;
}

static void wait64(const struct ck_ec_wait_state *state, const uint64_t *addr,
		   uint64_t expected, const struct timespec *deadline)
{
	(void)addr;
	(void)expected;
	(void)deadline;

	assert(state->ops == &test_ops);
	return;
}

static void wake32(const struct ck_ec_ops *ops, const uint32_t *addr)
{
	(void)addr;

	assert(ops == &test_ops);
	woken = true;
	return;
}

static void wake64(const struct ck_ec_ops *ops, const uint64_t *addr)
{
	(void)addr;

	assert(ops == &test_ops);
	woken = true;
	return;
}

/*
 * Check that adding a value calls the wake function when the sign bit
 * is set, and does not call it when the sign bit is unset (modulo
 * wrap-around).
 */
struct example {
	uint64_t initial;
	uint64_t increment;
};

const struct example examples[] = {
	{ INT32_MAX, 0 },
	{ INT32_MAX, 1 },
	{ 0 + (0U << 31), 0 },
	{ 1 + (0U << 31), 0 },
	{ 0 + (1U << 31), 0 },
	{ 1 + (1U << 31), 0 },

	{ 0 + (0U << 31), 1 },
	{ 1 + (0U << 31), 1 },
	{ 0 + (1U << 31), 1 },
	{ 1 + (1U << 31), 1 },

	{ 0 + (0U << 31), INT32_MAX },
	{ 1 + (0U << 31), INT32_MAX },
	{ 0 + (1U << 31), INT32_MAX },
	{ 1 + (1U << 31), INT32_MAX },

	{ INT64_MAX, 0 },
	{ INT64_MAX, 1 },
	{ 0 + (0ULL << 63), 0 },
	{ 1 + (0ULL << 63), 0 },
	{ 0 + (1ULL << 63), 0 },
	{ 1 + (1ULL << 63), 0 },

	{ 0 + (0ULL << 63), 1 },
	{ 1 + (0ULL << 63), 1 },
	{ 0 + (1ULL << 63), 1 },
	{ 1 + (1ULL << 63), 1 },

	{ 0 + (0ULL << 63), INT64_MAX },
	{ 1 + (0ULL << 63), INT64_MAX },
	{ 0 + (1ULL << 63), INT64_MAX },
	{ 1 + (1ULL << 63), INT64_MAX },
};

static inline int test_wakeup(const struct example *example)
{
	for (size_t i = 0; i < 2; i++) {
		const struct ck_ec_mode *mode = &modes[i];
		const uint32_t increment = example->increment & INT32_MAX;
		struct ck_ec32 ec;
		bool should_wake;
		bool may_wake;

		ec.counter = example->initial;
		should_wake = increment != 0 && (ec.counter & (1U << 31));
		may_wake = should_wake || (ec.counter & (1U << 31));

		woken = false;
		ck_ec32_add(&ec, mode, increment);
		assert(!should_wake || woken);
		assert(may_wake || !woken);
		assert(!woken || ck_ec32_has_waiters(&ec) == false);

		/* Test inc now. */
		ec.counter = example->initial + increment;
		should_wake = ec.counter & (1U << 31);
		may_wake = should_wake || ((ec.counter + 1) & (1U << 31));

		woken = false;
		ck_ec32_inc(&ec, mode);
		assert(!should_wake || woken);
		assert(may_wake || !woken);
		assert(!woken || ck_ec32_has_waiters(&ec) == false);
	}

#ifdef CK_F_EC64
	for (size_t i = 0; i < 2; i++) {
		const struct ck_ec_mode *mode = &modes[i];
		const uint64_t increment = example->increment & INT64_MAX;
		struct ck_ec64 ec;
		bool should_wake;
		bool may_wake;

		ec.counter = example->initial;
		should_wake = increment != 0 && (ec.counter & 1);
		may_wake = should_wake || (ec.counter & 1);

		woken = false;
		ck_ec64_add(&ec, mode, increment);
		assert(!should_wake || woken);
		assert(may_wake || !woken);
		assert(!woken || ck_ec64_has_waiters(&ec) == false);

		/* Test inc now. */
		ec.counter = example->initial + increment;
		should_wake = ec.counter & 1;

		woken = false;
		ck_ec64_inc(&ec, mode);
		assert(should_wake == woken);
		assert(!woken || ck_ec64_has_waiters(&ec) == false);
	}
#endif /* CK_F_EC64 */

	return 0;
}

TEST(test_wakeup, examples)

#include <assert.h>
#include <ck_ec.h>

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

static int gettime(const struct ck_ec_ops *ops, struct timespec *out)
{
	(void)out;

	assert(ops == &test_ops);
	return -1;
}

static void wait32(const struct ck_ec_wait_state *wait_state,
		   const uint32_t *addr, uint32_t expected,
		   const struct timespec *deadline)
{
	(void)addr;
	(void)expected;
	(void)deadline;

	assert(wait_state->ops == &test_ops);
	return;
}

static void wait64(const struct ck_ec_wait_state *wait_state,
		   const uint64_t *addr, uint64_t expected,
		   const struct timespec *deadline)
{
	(void)addr;
	(void)expected;
	(void)deadline;

	assert(wait_state->ops == &test_ops);
	return;
}

static void wake32(const struct ck_ec_ops *ops, const uint32_t *addr)
{
	(void)addr;

	assert(ops == &test_ops);
	return;
}

static void wake64(const struct ck_ec_ops *ops, const uint64_t *addr)
{
	(void)addr;

	assert(ops == &test_ops);
	return;
}

/*
 * Check that calling ck_ec{32,64}_wake always clears the waiting bit.
 */

struct example {
	uint64_t value;
};

const struct example examples[] = {
	{ 0 },
	{ 1 },
	{ 1UL << 30 },
	{ 1UL << 31 },
	{ INT32_MAX },
	{ INT64_MAX },
	{ 1ULL << 62 },
	{ 1ULL << 63 },
};

static inline int test_slow_wakeup(const struct example *example)
{
	{
		struct ck_ec32 ec = { .counter = example->value };

		ck_ec32_wake(&ec, &test_ops);
		assert(!ck_ec32_has_waiters(&ec));
	}

#ifdef CK_F_EC64
	{
		struct ck_ec64 ec = { .counter = example->value };

		ck_ec64_wake(&ec, &test_ops);
		assert(!ck_ec64_has_waiters(&ec));
	}
#endif /* CK_F_EC64 */

	return 0;
}

TEST(test_slow_wakeup, examples)

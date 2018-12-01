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
 * Check that adding a value correctly updates the counter, and that
 * incrementing after that also works.
 */
struct example {
	uint64_t value[2];
};

static const struct example examples[] = {
	{ { 0, 0 } },
	{ { 1, 2 } },
	{ { 0, INT32_MAX - 2 } },
	{ { 0, INT32_MAX - 1 } },
	{ { 0, INT32_MAX } },
	{ { 0, INT64_MAX - 2 } },
	{ { 0, INT64_MAX - 1 } },
	{ { 0, INT64_MAX } },
};

static inline int test_value(const struct example *example)
{
	for (size_t i = 0; i < 2; i++) {
		const struct ck_ec_mode *mode = &modes[i];
		const uint32_t value0 = example->value[0] & INT32_MAX;
		const uint32_t value1 = example->value[1] & INT32_MAX;
		struct ck_ec32 ec;

		ck_ec32_init(&ec, 0);
		assert(ck_ec32_value(&ec) == 0);

		ck_ec32_add(&ec, mode, value0);
		assert(ck_ec32_value(&ec) == value0);

		ck_ec32_add(&ec, mode, value1);
		assert(ck_ec32_value(&ec) ==
		   ((value0 + value1) & INT32_MAX));


		ck_ec32_inc(&ec, mode);
		assert(ck_ec32_value(&ec) ==
		   ((value0 + value1 + 1) & INT32_MAX));
	}

#ifdef CK_F_EC64
	for (size_t i = 0; i < 2; i++) {
		const struct ck_ec_mode *mode = &modes[i];
		const uint64_t value0 = example->value[0] & INT64_MAX;
		const uint64_t value1 = example->value[1] & INT64_MAX;
		struct ck_ec64 ec;

		ck_ec64_init(&ec, 0);
		assert(ck_ec64_value(&ec) == 0);

		ck_ec64_add(&ec, mode, value0);
		assert(ck_ec64_value(&ec) == value0);

		ck_ec64_add(&ec, mode, value1);
		assert(ck_ec64_value(&ec) ==
		   ((value0 + value1) & INT64_MAX));

		ck_ec64_inc(&ec, mode);
		assert(ck_ec64_value(&ec) ==
		   ((value0 + value1 + 1) & INT64_MAX));
	}
#endif /* CK_F_EC64 */

	return 0;
}

TEST(test_value, examples)

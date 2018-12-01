#include <assert.h>

#include "../../../src/ck_ec_timeutil.h"
#include "fuzz_harness.h"

struct example {
	uint32_t nsec;
	uint32_t multiplier;
	unsigned int shift;
};

static const struct example examples[] = {
	{
		UINT32_MAX,
		UINT32_MAX,
		1
	},
	{
		10,
		20,
		0
	}
};

static inline int test_wait_time_scale(const struct example *example)
{
	const uint32_t nsec = example->nsec;
	const uint32_t multiplier = example->multiplier;
	const unsigned int shift = example->shift % 32;
	uint32_t actual = wait_time_scale(nsec, multiplier, shift);
	uint64_t expected = ((uint64_t)nsec * multiplier) >> shift;

	if (expected > UINT32_MAX) {
		expected = UINT32_MAX;
	}

	assert(actual == expected);
	return 0;
}

TEST(test_wait_time_scale, examples)

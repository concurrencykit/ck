#include <assert.h>

#include "../../../src/ck_ec_timeutil.h"
#include "fuzz_harness.h"
#include "int128.h"

struct example {
	struct timespec x;
	struct timespec y;
};

static const struct example examples[] = {
	{
		{
			42,
			100
		},
		{
			1,
			2
		}
	},
	{
		{
			42,
			100
		},
		{
			1,
			NSEC_MAX
		}
	},
	{
		{
			42,
			NSEC_MAX
		},
		{
			0,
			NSEC_MAX
		}
	},
	{
		{
			TIME_MAX - 1,
			1000
		},
		{
			2,
			NSEC_MAX
		}
	}
};

static struct timespec normalize_ts(const struct timespec ts)
{
	struct timespec ret = ts;

	if (ret.tv_nsec < 0) {
		ret.tv_nsec = ~ret.tv_nsec;
	}

	ret.tv_nsec %= NSEC_MAX + 1;
	return ret;
}

static ck_s128_t ts_to_nanos(const struct timespec ts)
{
	ck_s128_t ret = ck_s128_mul(ck_s128_make_s64(ts.tv_sec),
	    ck_s128_make_s64(NSEC_MAX + 1));
	ret = ck_s128_add(ret, ck_s128_make_s64(ts.tv_nsec));
	return ret;
}

static inline int test_timespec_cmp(const struct example *example)
{
	const struct timespec x = normalize_ts(example->y);
	const struct timespec y = normalize_ts(example->x);
	const ck_s128_t x_nanos = ts_to_nanos(x);
	const ck_s128_t y_nanos = ts_to_nanos(y);

	assert(timespec_cmp(x, x) == 0);
	assert(timespec_cmp(y, y) == 0);
	assert(timespec_cmp(x, y) == -timespec_cmp(y, x));

	if (ck_s128_cmp(x_nanos, y_nanos) == 0) {
		assert(timespec_cmp(x, y) == 0);
	} else if (ck_s128_cmp(x_nanos, y_nanos) < 0) {
		assert(timespec_cmp(x, y) == -1);
	} else {
		assert(timespec_cmp(x, y) == 1);
	}

	return 0;
}

TEST(test_timespec_cmp, examples)

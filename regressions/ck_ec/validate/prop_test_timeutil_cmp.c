#include <assert.h>

#include "../../../src/ck_ec_timeutil.h"
#include "fuzz_harness.h"

#if ULONG_MAX > 4294967295
typedef __int128 dsword_t;
#else
typedef int64_t dsword_t;
#endif

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

static dsword_t ts_to_nanos(const struct timespec ts)
{
	return (dsword_t)ts.tv_sec * (NSEC_MAX + 1) + ts.tv_nsec;
}

static inline int test_timespec_cmp(const struct example *example)
{
	const struct timespec x = normalize_ts(example->y);
	const struct timespec y = normalize_ts(example->x);
	const dsword_t x_nanos = ts_to_nanos(x);
	const dsword_t y_nanos = ts_to_nanos(y);

	assert(timespec_cmp(x, x) == 0);
	assert(timespec_cmp(y, y) == 0);
	assert(timespec_cmp(x, y) == -timespec_cmp(y, x));

	if (x_nanos == y_nanos) {
		assert(timespec_cmp(x, y) == 0);
	} else if (x_nanos < y_nanos) {
		assert(timespec_cmp(x, y) == -1);
	} else {
		assert(timespec_cmp(x, y) == 1);
	}

	return 0;
}

TEST(test_timespec_cmp, examples)

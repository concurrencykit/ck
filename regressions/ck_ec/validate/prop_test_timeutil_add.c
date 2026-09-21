#include <assert.h>
#include <ck_stdint.h>

#include "../../../src/ck_ec_timeutil.h"
#include "fuzz_harness.h"
#include "int128.h"

struct example {
	struct timespec ts;
	struct timespec inc;
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

	if (ret.tv_sec < 0) {
		ret.tv_sec = ~ret.tv_sec;
	}

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

static inline int test_timespec_add(const struct example *example)
{
	const struct timespec ts = normalize_ts(example->ts);
	const struct timespec inc = normalize_ts(example->inc);
	const struct timespec actual = timespec_add(ts, inc);
	const ck_s128_t nanos = ck_s128_add(ts_to_nanos(ts), ts_to_nanos(inc));

	const ck_s128_t ceiling = ts_to_nanos(
	    (struct timespec) { TIME_MAX, NSEC_MAX });

	if (ck_s128_cmp(nanos, ceiling) > 0) {
		assert(actual.tv_sec == TIME_MAX);
		assert(actual.tv_nsec == NSEC_MAX);
	} else {
		assert(ck_s128_cmp(ts_to_nanos(actual), nanos) == 0);
		assert(actual.tv_nsec >= 0);
		assert(actual.tv_nsec <= NSEC_MAX);
	}

	return 0;
}

TEST(test_timespec_add, examples)

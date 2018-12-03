#include <assert.h>
#include <ck_limits.h>
#include <ck_stdint.h>

#include "../../../src/ck_ec_timeutil.h"
#include "fuzz_harness.h"

#if ULONG_MAX > 4294967295
typedef unsigned __int128 dword_t;
#else
typedef uint64_t dword_t;
#endif

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

static dword_t ts_to_nanos(const struct timespec ts)
{
	return (dword_t)ts.tv_sec * (NSEC_MAX + 1) + ts.tv_nsec;
}

static inline int test_timespec_add(const struct example *example)
{
	const struct timespec ts = normalize_ts(example->ts);
	const struct timespec inc = normalize_ts(example->inc);
	const struct timespec actual = timespec_add(ts, inc);
	const dword_t nanos = ts_to_nanos(ts) + ts_to_nanos(inc);

	if (nanos / (NSEC_MAX + 1) > TIME_MAX) {
		assert(actual.tv_sec == TIME_MAX);
		assert(actual.tv_nsec == NSEC_MAX);
	} else {
		assert(actual.tv_sec == (time_t)(nanos / (NSEC_MAX + 1)));
		assert(actual.tv_nsec == (long)(nanos % (NSEC_MAX + 1)));
	}

	return 0;
}

TEST(test_timespec_add, examples)

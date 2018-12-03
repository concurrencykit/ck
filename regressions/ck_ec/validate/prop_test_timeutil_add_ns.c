#include <assert.h>

#include "../../../src/ck_ec_timeutil.h"
#include "fuzz_harness.h"

#if ULONG_MAX > 4294967295
typedef unsigned __int128 dword_t;
#else
typedef uint64_t dword_t;
#endif

struct example {
	struct timespec ts;
	uint32_t ns;
};

static const struct example examples[] = {
	{
		{
			42,
			100
		},
		1
	},
	{
		{
			42,
			100
		},
		2 * NSEC_MAX
	},
	{
		{
			42,
			NSEC_MAX
		},
		NSEC_MAX
	},
	{
		{
			TIME_MAX - 1,
			1000
		},
		2 * NSEC_MAX
	}
};

static inline int test_timespec_add_ns(const struct example *example)
{
	struct timespec ts = {
		.tv_sec = example->ts.tv_sec,
		.tv_nsec = example->ts.tv_nsec
	};
	const uint32_t ns = example->ns;

	if (ts.tv_sec < 0) {
		ts.tv_sec = ~ts.tv_sec;
	}

	if (ts.tv_nsec < 0) {
		ts.tv_nsec = ~ts.tv_nsec;
	}

	ts.tv_nsec %= NSEC_MAX + 1;

	const struct timespec actual = timespec_add_ns(ts, ns);

	dword_t nanos =
	    (dword_t)ts.tv_sec * (NSEC_MAX + 1) + ts.tv_nsec;

	if (ns > NSEC_MAX) {
		nanos += NSEC_MAX + 1;
	} else {
		nanos += ns;
	}

	if (nanos / (NSEC_MAX + 1) > TIME_MAX) {
		assert(actual.tv_sec == TIME_MAX);
		assert(actual.tv_nsec == NSEC_MAX);
	} else {
		assert(actual.tv_sec == (time_t)(nanos / (NSEC_MAX + 1)));
		assert(actual.tv_nsec == (long)(nanos % (NSEC_MAX + 1)));
	}

	return 0;
}

TEST(test_timespec_add_ns, examples)

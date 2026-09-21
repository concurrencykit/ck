#ifndef CK_INT128_H
#define CK_INT128_H

#include <ck_stdint.h>

typedef struct ck_s128 {
	uint64_t hi;
	uint64_t lo;
} ck_s128_t;

static inline ck_s128_t
ck_s128_make_s64(int64_t value)
{
	return (ck_s128_t) {
		value < 0 ? (uint64_t)-1 : 0,
		(uint64_t)value,
	};
}

static inline int
ck_s128_cmp(ck_s128_t lhs, ck_s128_t rhs)
{
	if ((int64_t)lhs.hi < (int64_t)rhs.hi) {
		return -1;
	}

	if ((int64_t)lhs.hi > (int64_t)rhs.hi) {
		return 1;
	}

	if (lhs.lo < rhs.lo) {
		return -1;
	}

	if (lhs.lo > rhs.lo) {
		return 1;
	}

	return 0;
}

static inline ck_s128_t
ck_s128_add(ck_s128_t lhs, ck_s128_t rhs)
{
	return (ck_s128_t) {
		lhs.hi + rhs.hi + ((lhs.lo + rhs.lo) < lhs.lo),
		lhs.lo + rhs.lo,
	};
}

static inline ck_s128_t
ck_s128_mul(ck_s128_t lhs, ck_s128_t rhs)
{
	/*
	 * A s128 represents hi * 2^64 + lo. Multiplying two s128 values gives
	 * four terms:
	 *
	 *     lhs.lo * rhs.lo
	 *     lhs.hi * rhs.lo * 2^64
	 *     lhs.lo * rhs.hi * 2^64
	 *     lhs.hi * rhs.hi * 2^128
	 *
	 * We need only the low 128 bits, so the last term can be ignored. The
	 * middle two terms contribute only to result.hi.
	 *
	 * Computing lhs.lo * rhs.lo still requires a 128-bit result. We split
	 * both 64-bit values into 32-bit halves:
	 *
	 *     lhs.lo = a * 2^32 + b
	 *     rhs.lo = c * 2^32 + d
	 *
	 * Then:
	 *
	 *     lhs.lo * rhs.lo = a*c * 2^64 + (a*d + b*c) * 2^32 + b*d
	 *
	 * Inspired by Abseil's int128 implementation:
	 * https://github.com/abseil/abseil-cpp/blob/20260526.0/absl/numeric/int128.h#L1035
	 */
	uint64_t a = lhs.lo >> 32;
	uint64_t b = (uint32_t)lhs.lo;
	uint64_t c = rhs.lo >> 32;
	uint64_t d = (uint32_t)rhs.lo;

	ck_s128_t result = (ck_s128_t) {
		(lhs.hi * rhs.lo) + (lhs.lo * rhs.hi) + (a * c),
		(b * d),
	};
	result = ck_s128_add(result,
	    (ck_s128_t) { (a * d) >> 32, (a * d) << 32 });
	result = ck_s128_add(result,
	    (ck_s128_t) { (b * c) >> 32, (b * c) << 32 });

	return result;
}

#endif /* CK_INT128_H */

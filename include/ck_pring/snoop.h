#ifndef PRING_SNOOP_H
#define PRING_SNOOP_H
#include <ck_pring/common.h>

/**
 * Initialize a snooper block.  Snoopers are consumers that do not
 * block producers.  dep_begin, dep_end specifies a range of consumer
 * ids not to overtake.
 */
void
ck_pring_snoop_init(struct ck_pring_snooper *, const struct ck_pring *,
    uint32_t dep_begin, uint32_t dep_end);

/**
 * Approximately how many entries are available for snooping.  Only
 * useful if dependencies are active.
 */
static inline size_t
ck_pring_snoop_capacity(struct ck_pring_snooper *, const struct ck_pring *);

/**
 * Snoop the next value from the pring.
 *
 *  Return 0 on failure.
 */
static inline uintptr_t
ck_pring_snoop(struct ck_pring_snooper *, const struct ck_pring *);

/**
 * Snoop up to n values from the pring.
 *
 * Return the number of values snooped and written to dst.
 */
size_t
ck_pring_snoop_n(struct ck_pring_snooper *, const struct ck_pring *,
    uintptr_t *dst, size_t n);

/**
 * Inline implementation.
 */
static inline size_t
ck_pring_snoop_capacity(struct ck_pring_snooper *snoop,
    const struct ck_pring *ring)
{
	uint64_t cursor = snoop->cons.cursor;
	uint64_t limit = snoop->cons.read_limit;

	if (CK_CC_UNLIKELY((int64_t)(cursor - limit) >= 0)) {
		return ck_pring_consumer_update_limit(&snoop->cons, ring);
	}

	return limit - cursor;
}

uintptr_t
ck_pring_snoop_slow(struct ck_pring_snooper *snoop,
    const struct ck_pring *ring);

static inline uintptr_t
ck_pring_snoop(struct ck_pring_snooper *snoop, const struct ck_pring *ring)
{
	struct ck_pring_elt snap;
	const struct ck_pring_elt *buf = snoop->cons.buf;
	uint64_t mask = snoop->cons.mask;
	uint64_t cursor = snoop->cons.cursor;
	size_t loc = cursor & mask;

	if (CK_CC_UNLIKELY((int64_t)(cursor - snoop->cons.read_limit) >= 0)) {
		goto slow;
	}

	snap.gen = ck_pr_load_ptr(&buf[loc].gen);
	ck_pr_fence_load();
	/* read gen before value.  cursor is an upper bound on gen. */
	snap.value = ck_pr_load_ptr(&buf[loc].value);
	if (CK_CC_UNLIKELY((int64_t)((uint64_t)snap.gen - cursor) < 0)) {
		/* gen is too old. queue is still empty. */
		return 0;
	}

	ck_pr_fence_load();
	if (CK_CC_UNLIKELY((uint64_t)ck_pr_load_ptr(&buf[loc].gen) != cursor)) {
		/* gen doesn't match and/or cursor is out of date; try again. */
		goto slow;
	}

	snoop->cons.cursor = cursor + 1;
	return snap.value;

slow:
	return ck_pring_snoop_slow(snoop, ring);
}
#endif /* !PRING_SNOOP_H */

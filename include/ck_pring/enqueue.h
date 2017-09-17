#ifndef PRING_ENQUEUE_H
#define PRING_ENQUEUE_H
#include <ck_pring/common.h>

/**
 * Return an approximation of the remaining capacity in the ring.
 *
 * Exact for single producer.
 */
static inline size_t
ck_pring_enqueue_capacity(struct ck_pring *);

/**
 * Attempt to enqueue one value in a single-producer pring.
 *
 * Return true iff success.
 */
static inline bool
ck_pring_senqueue(struct ck_pring *, uintptr_t);

/**
 * Attempt to enqueue one value in a single-producer pring.
 *
 * Return true iff success, in which case old_value was overwritten
 * with the value previously in the ring buffer element.  The value
 * is unspecified on failure.
 */
static inline bool
ck_pring_senqueue_val(struct ck_pring *, uintptr_t, uintptr_t *old_value);

/**
 * Attempt to enqueue up to n values in a single-producer pring.
 *
 * Return the number of values enqueued; values[0 .. ret) is updated
 * with the value previously in the ring buffer elements.
 */
size_t
ck_pring_senqueue_n(struct ck_pring *ring, uintptr_t *values, size_t n);

/**
 * Attempt to enqueue one value in a multi-producer pring.
 *
 * Return true iff success.
 */
static inline bool
ck_pring_menqueue(struct ck_pring *, uintptr_t);

/**
 * Attempt to enqueue one value in a multi-producer pring.
 *
 * Return true iff success, in which case old_value was overwritten
 * with the value previously in the ring buffer element.  The value
 * is unspecified on failure.
 */
bool
ck_pring_menqueue_val(struct ck_pring *, uintptr_t, uintptr_t *old_value);

/**
 * Attempt to enqueue up to n values in a single-producer pring.
 *
 * Return the number of values enqueued; values[0 .. ret) is updated
 * with the value previously in the ring buffer elements.
 */
size_t
ck_pring_menqueue_n(struct ck_pring *, uintptr_t *values, size_t n);

/**
 * Inline implementation.
 */
static inline bool
ck_pring_menqueue(struct ck_pring *ring, uintptr_t value)
{
	uintptr_t buf;

	(void)buf;
	return ck_pring_menqueue_val(ring, value, &buf);
}

size_t
ck_pring_enqueue_capacity_slow(struct ck_pring *ring);

static inline size_t
ck_pring_enqueue_capacity(struct ck_pring *ring)
{
	uint64_t mask = ring->prod.mask;
	uint64_t consumer_snap = ring->prod.consumer_snap;
	uint64_t cursor = ring->prod.cursor;

	if (CK_CC_UNLIKELY(cursor - consumer_snap > mask)) {
		return ck_pring_enqueue_capacity_slow(ring);
	}

	return (consumer_snap + mask + 1) - cursor;
}

static inline bool
ck_pring_senqueue(struct ck_pring *ring, uintptr_t value)
{

	return ck_pring_senqueue_val(ring, value, &ring->prod.dummy);
}

bool
ck_pring_senqueue_val_slow(struct ck_pring *ring, uintptr_t value,
    uintptr_t *old_value);

static inline bool
ck_pring_senqueue_val(struct ck_pring *ring, uintptr_t value,
    uintptr_t *old_value)
{
	struct ck_pring_elt *buf = ring->prod.buf;
	struct ck_pring_elt *dst;
	uint64_t mask = ring->prod.mask;
	/* only writer to prod.* is us. */
	uint64_t consumer_snap = ring->prod.consumer_snap;
	uint64_t cursor = ring->prod.cursor;
	size_t loc = cursor & mask;

	/*
	 * We know where we want to write.  Make sure our snapshot of
	 * the consumer cursor lets us write there (or update the
	 * snapshot), and write the value *before* publishing
	 * the new generation.
	 */
	dst = &buf[loc];
#ifdef __GNUC__
	__asm__("" : "+r"(dst)); /* compute dst before the branch. */
#endif
	if (CK_CC_UNLIKELY((cursor - consumer_snap) > mask)) {
		return ck_pring_senqueue_val_slow(ring, value, old_value);
	}

	/* We're not too far. do the write! */
	*old_value = dst->value;
	ck_pr_store_ptr((void **)&dst->value, (void *)value);
	ck_pr_fence_store();
	ck_pr_store_ptr(&dst->gen, (void *)cursor);

	ck_pr_fence_store();
	ck_pr_store_64(&ring->prod.cursor, cursor + 1);
	return true;
}
#endif /* !PRING_ENQUEUE_H */

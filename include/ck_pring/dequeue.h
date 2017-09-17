#ifndef PRING_DEQUEUE_H
#define PRING_DEQUEUE_H
#include <ck_pring/common.h>

/**
 * Approximately how many entries are available for consumption.  Only
 * useful if dependencies are active.
 */
static inline size_t
ck_pring_consume_capacity(struct ck_pring *, size_t index);

/**
 * Dequeue a value from a single-threaded consumer block.
 *
 * Return 0 if there is no new value in the pring.
 */
static inline uintptr_t
ck_pring_sdequeue(struct ck_pring *, size_t index);

/**
 * Read a value from a single-threaded consumer block.
 *
 * Return 0 if there is no new value in the pring.
 */
static inline uintptr_t
ck_pring_sread(struct ck_pring *, size_t index);

/**
 * Consume the last value returned by sread for a single-thread consumer block.
 */
static inline void
ck_pring_sconsume(struct ck_pring *, size_t index);

/**
 * Dequeue up to n values from a single-thread consumer block.
 *
 * Return the number of values read and written to dst.
 */
size_t
ck_pring_sdequeue_n(struct ck_pring *, size_t index, uintptr_t *dst, size_t n);

/**
 * Read up to n values from a single-thread consumer block.
 *
 * Return the number of values read and written to dst.
 */
size_t
ck_pring_sread_n(struct ck_pring *, size_t index, uintptr_t *dst, size_t n);

/**
 * Consume the last n values returned by sread_n.
 */
static inline void
ck_pring_sconsume_n(struct ck_pring *, size_t index, size_t n);

/**
 * Dequeue a value from a multi-consumer block.
 *
 * Return 0 if there is no new value.
 */
static inline uintptr_t
ck_pring_mdequeue(struct ck_pring *, size_t index);

static inline uintptr_t
ck_pring_mtrydequeue(struct ck_pring *, size_t index);

static inline uintptr_t
ck_pring_mread(struct ck_pring *, size_t index, uint64_t *OUT_gen);

static inline uintptr_t
ck_pring_mtryread(struct ck_pring *, size_t index, uint64_t *OUT_gen);

static inline bool
ck_pring_mconsume(struct ck_pring *, size_t index, uint64_t gen);

static inline size_t
ck_pring_mdequeue_n(struct ck_pring *, size_t index, uintptr_t *dst, size_t n);

static inline size_t
ck_pring_mtrydequeue_n(struct ck_pring *, size_t index, uintptr_t *dst, size_t n);

static inline size_t
ck_pring_mread_n(struct ck_pring *, size_t index,
    uintptr_t *dst, size_t n, uint64_t *OUT_gen);

static inline size_t
ck_pring_mtryread_n(struct ck_pring *, size_t index,
    uintptr_t *dst, size_t n, uint64_t *OUT_gen);

static inline bool
ck_pring_mconsume_n(struct ck_pring *, size_t index, uint64_t gen, size_t n);

/**
 * Inline implementation.
 */
static inline size_t
ck_pring_consume_capacity(struct ck_pring *ring, size_t index)
{
	struct ck_pring_consumer *cons = ck_pring_consumer_by_id(ring, index);
	uint64_t cursor = cons->cursor;
	uint64_t limit = cons->read_limit;

	if (CK_CC_UNLIKELY((int64_t)(cursor - limit) >= 0)) {
		return ck_pring_consumer_update_limit(cons, ring);
	}

	return limit - cursor;
}

uintptr_t ck_pring_sdequeue_slow(struct ck_pring *, size_t);

static inline uintptr_t
ck_pring_sdequeue(struct ck_pring *ring, size_t index)
{
	struct ck_pring_elt snap;
	struct ck_pring_consumer *cons = ck_pring_consumer_by_id(ring, index);
	const struct ck_pring_elt *buf = cons->buf;
	uint64_t mask = cons->mask;
	uint64_t cursor = cons->cursor; /* only writer is us. */
	size_t loc = cursor & mask;

	if (CK_CC_UNLIKELY((int64_t)(cursor - cons->read_limit) >= 0)) {
		/*
		 * This only happens with dependencies, when consumers
		 * catch up to the parents.  This will always be
		 * complex, and I don't feel bad about slowing down
		 * consumers that are going too fast.
		 */
		return ck_pring_sdequeue_slow(ring, index);
	}

	/*
	 * We know where the next element we wish to consume lives.
	 * Load its generation *before* its value.  If the generation
	 * matches our cursor, consume the element and return the
	 * value.
	 */
	snap.gen = ck_pr_load_ptr(&buf[loc].gen);
#ifdef assert
	assert((int64_t)((uint64_t)snap.gen - cursor) <= 0 &&
	    "Concurrent dequeue in sdequeue?");
#endif
	ck_pr_fence_load();
	/* read gen before value.  cursor is always an upper bound on gen. */
	snap.value = ck_pr_load_ptr(&buf[loc].value);
	if (CK_CC_UNLIKELY((uint64_t)snap.gen != cursor)) {
		/*
		 * This will tend to be either always false (normal
		 * operations) or always true (queue is empty); a cmov
		 * would be easy but would be slightly slower than a
		 * predictable branch in both cases.
		 */
		return 0;
	}

	ck_pr_fence_load_store();
	/*
	 * Producers will read cons.cursor. Make sure to consume the
	 * cell's value before releasing, and make sure cursor is safe
	 * to read by other threads.
	 */
	ck_pr_store_64(&cons->cursor, cursor + 1);
	return snap.value;
}

uintptr_t ck_pring_sread_slow(struct ck_pring *, size_t);

static inline uintptr_t
ck_pring_sread(struct ck_pring *ring, size_t index)
{
	struct ck_pring_elt snap;
	struct ck_pring_consumer *cons = ck_pring_consumer_by_id(ring, index);
	const struct ck_pring_elt *buf = cons->buf;
	uint64_t mask = cons->mask;
	uint64_t cursor = cons->cursor;
	size_t loc = cursor & mask;

	if (CK_CC_UNLIKELY((int64_t)(cursor - cons->read_limit) >= 0)) {
		return ck_pring_sread_slow(ring, index);
	}

	snap.gen = ck_pr_load_ptr(&buf[loc].gen);
#ifdef assert
	assert((int64_t)((uint64_t)snap.gen - cursor) <= 0 &&
	    "Concurrent dequeue in sread?");
#endif
	ck_pr_fence_load();
	snap.value = ck_pr_load_ptr(&buf[loc].value);
	return ((uint64_t)snap.gen == cursor) ? snap.value : 0;
}

static inline void
ck_pring_sconsume(struct ck_pring *ring, size_t index)
{

	ck_pring_sconsume_n(ring, index, 1);
	return;
}

static inline void
ck_pring_sconsume_n(struct ck_pring *ring, size_t index, size_t n)
{
	struct ck_pring_consumer *cons = ck_pring_consumer_by_id(ring, index);

	ck_pr_fence_load_store();
	ck_pr_store_64(&cons->cursor, ring->cons.cons.cursor + n);
	return;
}

uintptr_t
ck_pring_mdequeue_generic(struct ck_pring *ring, size_t index, bool hard);

static inline uintptr_t
ck_pring_mdequeue(struct ck_pring *ring, size_t index)
{

	return ck_pring_mdequeue_generic(ring, index, true);
}

static inline uintptr_t
ck_pring_mtrydequeue(struct ck_pring *ring, size_t index)
{

	return ck_pring_mdequeue_generic(ring, index, false);
}

uintptr_t ck_pring_mread_slow(struct ck_pring *, size_t, uint64_t *, bool);

static inline uintptr_t
ck_pring_mread_generic(struct ck_pring *ring, size_t index,
    uint64_t *OUT_gen, bool hard)
{
	struct ck_pring_elt snap;
	struct ck_pring_consumer *cons = ck_pring_consumer_by_id(ring, index);
	const struct ck_pring_elt *buf = cons->buf;
	uint64_t mask = cons->mask;
	uint64_t cursor = ck_pr_load_64(&cons->cursor);
	uint64_t read_limit = ck_pr_load_64(&cons->read_limit);
	size_t loc = cursor & mask;

	if (CK_CC_UNLIKELY((int64_t)(cursor - read_limit) >= 0)) {
		goto slow;
	}

	snap.gen = ck_pr_load_ptr(&buf[loc].gen);
	ck_pr_fence_load();
	snap.value = ck_pr_load_ptr(&buf[loc].value);
	if (CK_CC_UNLIKELY((int64_t)((uint64_t)snap.gen - cursor) < 0)) {
		return 0;
	}

	ck_pr_fence_load();
	if (CK_CC_UNLIKELY((uint64_t)ck_pr_load_ptr(&buf[loc].gen) != cursor)) {
		goto slow;
	}

	return snap.value;

slow:
	return ck_pring_mread_slow(ring, index, OUT_gen, hard);
}

static inline uintptr_t
ck_pring_mread(struct ck_pring *ring, size_t index, uint64_t *OUT_gen)
{

	return ck_pring_mread_generic(ring, index, OUT_gen, true);
}

static inline uintptr_t
ck_pring_mtryread(struct ck_pring *ring, size_t index, uint64_t *OUT_gen)
{

	return ck_pring_mread_generic(ring, index, OUT_gen, false);
}

static inline bool
ck_pring_mconsume(struct ck_pring *ring, size_t index, uint64_t gen)
{

	return ck_pring_mconsume_n(ring, index, gen, 1);
}

size_t
ck_pring_mdequeue_n_generic(struct ck_pring *ring, size_t index,
    uintptr_t *dst, size_t n, bool hard);

inline size_t
ck_pring_mdequeue_n(struct ck_pring *ring, size_t index, uintptr_t *dst, size_t n)
{

	return ck_pring_mdequeue_n_generic(ring, index, dst, n, true);
}

inline size_t
ck_pring_mtrydequeue_n(struct ck_pring *ring, size_t index, uintptr_t *dst, size_t n)
{

	return ck_pring_mdequeue_n_generic(ring, index, dst, n, false);
}

static inline bool
ck_pring_mconsume_n(struct ck_pring *ring, size_t index, uint64_t gen, size_t n)
{
	struct ck_pring_consumer *cons = ck_pring_consumer_by_id(ring, index);

	ck_pr_fence_load_store();
	return ck_pr_cas_64(&cons->cursor, gen, gen + n);
}

size_t
ck_pring_mread_n_generic(struct ck_pring *ring, size_t index,
    uintptr_t *dst, size_t n, uint64_t *OUT_gen, bool hard);

static inline size_t
ck_pring_mread_n(struct ck_pring *ring, size_t index,
    uintptr_t *dst, size_t n, uint64_t *OUT_gen)
{

	return ck_pring_mread_n_generic(ring, index,
	    dst, n, OUT_gen, true);
}

static inline size_t
ck_pring_mtryread_n(struct ck_pring *ring, size_t index,
    uintptr_t *dst, size_t n, uint64_t *OUT_gen)
{

	return ck_pring_mread_n_generic(ring, index,
	    dst, n, OUT_gen, false);
}
#endif /* !PRING_DEQUEUE_H */

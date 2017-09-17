#include <assert.h>
#include <ck_pring/enqueue.h>

#include <ck_cc.h>
#include <ck_pr.h>
#include <ck_limits.h>
#include <sys/types.h>

static uint64_t
oldest_consumer_snap(const struct ck_pring *ring, uint64_t cursor)
{
	const struct ck_pring_consumer_block *consumers = &ring->cons;
	size_t n_consumer = ring->prod.n_consumer;
	uint64_t ret = cursor;

	for (size_t i = n_consumer; i --> 0; ) {
		const struct ck_pring_consumer *current = &consumers[i].cons;
		uint64_t current_cursor = ck_pr_load_64(&current->cursor);
		size_t skip = current->dependency_begin;

		if ((int64_t)(current_cursor - ret) < 0) {
			ret = current_cursor;
		}

		/*
		 * Current cursor includes [begin, end).  If end >= i,
		 * we may skip everything down to and including begin:
		 * all skipped indices are covered by the current
		 * cursor.
		 */
		i = (current->dependency_end >= i) ? skip : i;
	}

	return ret;
}

size_t
ck_pring_enqueue_capacity_slow(struct ck_pring *ring)
{
	uint64_t mask = ring->prod.mask;
	uint64_t consumer_snap;
	uint64_t cursor = ring->prod.cursor;

	consumer_snap = oldest_consumer_snap(ring, cursor);
	ck_pr_store_64(&ring->prod.consumer_snap, consumer_snap);
	if (cursor - consumer_snap > mask) {
		return 0;
	}

	return (consumer_snap + mask + 1) - cursor;
}

bool
ck_pring_senqueue_val_slow(struct ck_pring *ring, uintptr_t value,
    uintptr_t *old_value)
{
	uint64_t mask = ring->prod.mask;
	uint64_t consumer_snap;
	uint64_t cursor = ring->prod.cursor;

	consumer_snap = oldest_consumer_snap(ring, cursor);
	ring->prod.consumer_snap = consumer_snap;
	if (cursor - consumer_snap > mask) {
		return false;
	}

	return ck_pring_senqueue_val(ring, value, old_value);
}

size_t
ck_pring_senqueue_n(struct ck_pring *ring, uintptr_t *values, size_t n)
{
	struct ck_pring_elt *buf = ring->prod.buf;
	uint64_t mask = ring->prod.mask;
	uint64_t base_cursor = ring->prod.cursor;
	size_t capacity;
	size_t produced;

	capacity = ck_pring_enqueue_capacity(ring);
	if (n > capacity) {
		n = capacity;
	}

	for (produced = 0; produced < n; produced++) {
		struct ck_pring_elt *dst;
		uint64_t cursor = base_cursor + produced;
		uintptr_t previous;
		size_t loc = cursor & mask;

		dst = &buf[loc];
		previous = dst->value;
		ck_pr_store_ptr((void **)&dst->value, (void *)values[produced]);
		ck_pr_fence_store();
		ck_pr_store_ptr(&dst->gen, (void *)cursor);

		values[produced] = previous;
	}

	ck_pr_fence_store();
	ck_pr_store_64(&ring->prod.cursor, base_cursor + produced);
	return produced;
}

/**
 * Assuming that cursor is safe to overwrite (i.e., is at most mask
 * ahead of the read-side cursor), attempt to overwrite an *older*
 * record with our new value.
 *
 * Ret < 0 means success.
 * Ret == 0 means we lost the race to update cursor.
 * Ret > 0 means we lost the race by (at least) a full revolution
 * around the ring buffer.
 */
static int64_t
try_menqueue_one(struct ck_pring_producer *snap,
    uintptr_t value,
    uint64_t cursor,
    uintptr_t *old)
{
	struct ck_pring_elt expected, update;
	struct ck_pring_elt *buf = snap->buf;
	uint64_t actual_gen;
	uint64_t mask = snap->mask;
	uint64_t loc = cursor & mask;
	int64_t ret;

	expected.value = ck_pr_load_ptr(&buf[loc].value);
	/* no barrier here: the CAS will just fail. */
	expected.gen = ck_pr_load_ptr(&buf[loc].gen);
	actual_gen = (uint64_t)expected.gen;

	ret = (int64_t)(actual_gen - cursor);
	if (ret >= 0) {
		/* we're trying to replace a fresh record. fail. */
		goto late;
	}

	update.value = value;
	update.gen = (void *)cursor;
	if (CK_CC_LIKELY(ck_pr_cas_ptr_2_value(&buf[loc], &expected, &update,
	    &expected))) {
		*old = expected.value;
		return ret;
	}

	/*
	 * if we failed, try again.  the dwcas gave us a consistent
	 * read, so no spurious failure here.
	 */
	actual_gen = (uint64_t)expected.gen;
	ret = (int64_t)(actual_gen - cursor);
	if (ret >= 0) {
		goto late;
	}

	if (ck_pr_cas_ptr_2_value(&buf[loc], &expected, &update,
	    &expected)) {
		*old = expected.value;
		return ret;
	}

	actual_gen = (uint64_t)expected.gen;
	ret = 0;

late:
	/*
	 * If we're late, we know the next "free" generation value is
	 * at least one more than the one we just observed.
	 */
	snap->cursor = actual_gen + 1;
	return ret;
}

/**
 * Bounded linear search for an empty cell, up to snap->cons.consumer_smap + mask;
 * Return true on success, false otherwise.
 * Update the snapshot's (write) cursor on failure.
 * Update the producer-side cache on success.
 */
static inline bool
try_menqueue(struct ck_pring *ring,
    struct ck_pring_producer *snap,
    uintptr_t value,
    uintptr_t *old)
{
	uint64_t consumer_snap = snap->consumer_snap;
	uint64_t cursor = snap->cursor;
	uint64_t mask = snap->mask;

	if ((int64_t)(cursor - consumer_snap) < 0) {
		cursor = consumer_snap;
	}

	for (; (cursor - consumer_snap) <= mask; cursor++) {
		int64_t ret;

		ret = try_menqueue_one(snap, value, cursor, old);
		if (ret > 0) {
			/*
			 * we're really off. break out of here and
			 * update our snapshots.  try_menqueue_one
			 * already updated snap->cursor.
			 */
			return false;
		}

		/*
		 * Success!
		 */
		if (ret < 0) {
			ck_pr_store_64(&ring->prod.cursor, cursor + 1);
			ck_pr_store_64(&ring->prod.consumer_snap,
			    snap->consumer_snap);
			return true;
		}
	}

	snap->cursor = cursor;
	return false;
}

bool
ck_pring_menqueue_val(struct ck_pring *ring, uintptr_t value,
    uintptr_t *old)
{
	struct ck_pring_producer snap;

	snap.buf = ring->prod.buf;
	snap.mask = ring->prod.mask;
	snap.consumer_snap = ck_pr_load_64(&ring->prod.consumer_snap);
	snap.cursor = ck_pr_load_64(&ring->prod.cursor);

	/*
	 * Fast path: snap.cursor isn't too far ahead. Immediately
	 * try to write there.
	 * We only access the producers' struct and the buffer.
	 */
	if (CK_CC_LIKELY((snap.cursor - snap.consumer_snap) <= snap.mask)) {
		if (CK_CC_LIKELY(
		    try_menqueue_one(&snap, value, snap.cursor, old) < 0)) {
			/* Success: racily update our local cursor and win. */
			ck_pr_store_64(&ring->prod.cursor, snap.cursor + 1);
			return true;
		}
	}

	/*
	 * Slow path: update our private snapshot from the producers'
	 * and consumers' structs, until the consumers' cursor stops
	 * moving (if that happens, it's really time to fail).
	 */
	for (;;) {
		uint64_t consumer_snap;
		uint64_t prod_cursor;

		/*
		 * Linear search for an empty cell that's OK to
		 * overwrite.  On success, nothing else to do:
		 * try_menqueue updates the producers' cache.
		 */
		if (try_menqueue(ring, &snap, value, old)) {
			return true;
		}

		prod_cursor = ck_pr_load_64(&ring->prod.cursor);
		/*
		 * prod.cursor is a racy hint.  Either update
		 * our cache or move the global cursor ahead.
		 */
		if ((int64_t)(prod_cursor - snap.cursor) > 0) {
			snap.cursor = prod_cursor;
		} else {
			ck_pr_store_64(&ring->prod.cursor, snap.cursor);
		}

		consumer_snap = oldest_consumer_snap(ring, snap.cursor);
		/* No progress on the consumer's end. Stop trying.*/
		if (consumer_snap == snap.consumer_snap) {
			uint64_t current_snap;

			/* Update the global snap if it's older than ours. */
			current_snap = ck_pr_load_64(&ring->prod.consumer_snap);
			if ((int64_t)(consumer_snap - current_snap) < 0) {
				ck_pr_store_64(&ring->prod.consumer_snap,
				    consumer_snap);
			}

			break;
		}

		snap.consumer_snap = consumer_snap;
	}

	return false;
}

size_t
ck_pring_menqueue_n(struct ck_pring *ring, uintptr_t *values, size_t n)
{

	/* XXX: do this better. */
	for (size_t i = 0; i < n; i++) {
		uintptr_t current = values[i];
		uintptr_t update;

		if (!ck_pring_menqueue_val(ring, current, &update)) {
			return i;
		}

		values[i] = update;
	}

	return n;
}

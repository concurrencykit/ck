#include <assert.h>
#include <ck_pring/dequeue.h>

#include <sys/types.h>

uintptr_t
ck_pring_sdequeue_slow(struct ck_pring *ring, size_t index)
{
	uintptr_t buf[1] = { 0 };
	uintptr_t n;

	n = ck_pring_sdequeue_n(ring, index, buf, 1);
	return buf[0] & -n;
}

uintptr_t
ck_pring_sread_slow(struct ck_pring *ring, size_t index)
{
	uintptr_t buf[1] = { 0 };
	uintptr_t n;

	n = ck_pring_sread_n(ring, index, buf, 1);
	return buf[0] & -n;
}

size_t
ck_pring_sdequeue_n(struct ck_pring *ring, size_t index,
    uintptr_t *dst, size_t n)
{
	size_t read;

	read = ck_pring_sread_n(ring, index, dst, n);
	ck_pring_sconsume_n(ring, index, read);
	return read;
}

size_t
ck_pring_sread_n(struct ck_pring *ring, size_t index,
    uintptr_t *dst, size_t n)
{
	struct ck_pring_consumer *cons = ck_pring_consumer_by_id(ring, index);
	const struct ck_pring_elt *buf = cons->buf;
	uint64_t mask = cons->mask;
	uint64_t base_cursor = cons->cursor; /* only writer is us. */
	uint64_t read_limit = ck_pr_load_64(&cons->read_limit);
	size_t capacity = read_limit - base_cursor;
	size_t consumed;

	if (CK_CC_UNLIKELY((int64_t)(base_cursor - read_limit) >= 0)) {
		capacity = ck_pring_consumer_update_limit(cons, ring);
		if (capacity == 0) {
			return capacity;
		}
	}

	n = (n > capacity) ? capacity : n;
	/*
	 * No check for n == 0.  This situation should be rare, and
	 * the code below correctly handles it.
	 */

	/*
	 * See if we can immediately read n values.  We know values
	 * from base_cursor onward have not been overwritten.  We only
	 * have to check if the last item we wish to read has been
	 * produced before copying everything.
	 */
	{
		struct ck_pring_elt snap;
		uint64_t last_cursor = base_cursor + n - 1;
		size_t last_loc = last_cursor & mask;

		snap.gen = ck_pr_load_ptr(&buf[last_loc].gen);
		ck_pr_fence_load();
		if (CK_CC_UNLIKELY((uint64_t)snap.gen != last_cursor)) {
			goto slow;
		}

		for (size_t i = 0; i < n; i++) {
			uint64_t cursor = base_cursor + i;
			size_t loc = cursor & mask;

			dst[i] = ck_pr_load_ptr(&buf[loc].value);
		}

		return n;
	}

slow:
	for (consumed = 0; consumed < n; consumed++) {
		struct ck_pring_elt snap;
		uint64_t cursor = base_cursor + consumed;
		size_t loc = cursor & mask;

		snap.gen = ck_pr_load_ptr(&buf[loc].gen);
		ck_pr_fence_load();
		snap.value = ck_pr_load_ptr(&buf[loc].value);
		if ((uint64_t)snap.gen != cursor) {
			assert((int64_t)((uint64_t)snap.gen - cursor) <= 0 &&
			    "Concurrent dequeue in sdequeue?");
			break;
		}

		dst[consumed] = snap.value;
	}

	return consumed;
}

/**
 * Update snap with the value for cursor in the ring buffer.
 * Returns a comparator value for generation ?= cursor.
 * 0 means we have the value we're looking for.
 * Negative means the value is older than expected (the cell
 * is empty).
 * Positive means the value is younger than expected (we
 * were too slow and lost the race).
 */
static inline int64_t
preacquire(struct ck_pring_elt *snap,
    const struct ck_pring_elt *buf, uint64_t mask, uint64_t cursor)
{
	size_t loc = cursor & mask;

	snap->gen = ck_pr_load_ptr(&buf[loc].gen);
	ck_pr_fence_load();
	snap->value = ck_pr_load_ptr(&buf[loc].value);

	return (int64_t)((uint64_t)snap->gen - cursor);
}

uintptr_t
ck_pring_mdequeue_generic(struct ck_pring *ring, size_t index, bool hard)
{
	struct ck_pring_elt snap;
	struct ck_pring_consumer *cons = ck_pring_consumer_by_id(ring, index);
	const struct ck_pring_elt *buf = cons->buf;
	uint64_t mask = cons->mask;
	uint64_t cursor = ck_pr_load_64(&cons->cursor);
	uint64_t read_limit = ck_pr_load_64(&cons->read_limit);

	if (CK_CC_UNLIKELY((int64_t)(cursor - read_limit) >= 0)) {
		uintptr_t ret;

		ret = ck_pring_consumer_update_limit(cons, ring);
		if (ret == 0) {
			return ret;
		}
	}

	/* Fast path, assuming our cursor is up to date. */
	{
		int64_t ret;

		ret = preacquire(&snap, buf, mask, cursor);
		if (CK_CC_LIKELY(ret == 0)) {
			/*
			 * The ring buffer element is up to date.
			 * Attempt to acquire it!
			 */
			if (CK_CC_LIKELY(ck_pr_cas_64(&cons->cursor,
			    cursor, cursor + 1))) {
				return snap.value;
			}
		} else if (CK_CC_LIKELY(ret < 0)) {
			/*
			 * The ring buffer element is too old (still
			 * empty).  Fail immediately.
			 */
			return 0;
		}
	}

	{
		uintptr_t arr[1] = { 0 };
		uintptr_t n;

		n = ck_pring_mdequeue_n_generic(ring, index, arr, 1, hard);
		return arr[0] & -n;
	}
}

uintptr_t
ck_pring_mread_slow(struct ck_pring *ring, size_t index,
    uint64_t *OUT_gen, bool hard)
{
	uintptr_t buf[1];
	uintptr_t n;

	n = ck_pring_mread_n_generic(ring, index, buf, 1, OUT_gen, hard);
	return buf[0] & -n;
}

size_t
ck_pring_mdequeue_n_generic(struct ck_pring *ring, size_t index,
    uintptr_t *dst, size_t n, bool hard)
{

	for (;;) {
		uint64_t gen;
		size_t ret;

		ret = ck_pring_mread_n_generic(ring, index, dst, n, &gen, hard);
		if (ret == 0 || ck_pring_mconsume_n(ring, index, gen, ret)) {
			return ret;
		}

		if (!hard) {
			return 0;
		}

		n = (n + 1) / 2;
	}

	return 0;
}

size_t
ck_pring_mread_n_generic(struct ck_pring *ring, size_t index,
    uintptr_t *dst, size_t n, uint64_t *OUT_gen, bool hard)
{
	struct ck_pring_consumer *cons = ck_pring_consumer_by_id(ring, index);
	const struct ck_pring_elt *buf = cons->buf;
	uint64_t mask = cons->mask;
	uint64_t base_cursor;
	uint64_t read_limit;
	size_t base_loc;
	size_t capacity;
	size_t consumed;

retry:
	base_cursor = ck_pr_load_64(&cons->cursor);
	read_limit = ck_pr_load_64(&cons->read_limit);
	base_loc = base_cursor & mask;
	capacity = read_limit - base_cursor;

	if (CK_CC_UNLIKELY((int64_t)(base_cursor - read_limit) >= 0)) {
		capacity = ck_pring_consumer_update_limit(cons, ring);
		if (capacity == 0) {
			return capacity;
		}
	}

	n = (n > capacity) ? capacity : n;
	*OUT_gen = base_cursor;

	{
		struct ck_pring_elt snap;
		uint64_t last_cursor = base_cursor + n - 1;
		size_t last_loc = last_cursor & mask;

		snap.gen = ck_pr_load_ptr(&buf[last_loc].gen);
		ck_pr_fence_load();
		if (CK_CC_UNLIKELY((uint64_t)snap.gen != last_cursor)) {
			goto slow;
		}

		for (size_t i = 0; i < n; i++) {
			uint64_t cursor = base_cursor + i;
			size_t loc = cursor & mask;

			dst[i] = ck_pr_load_ptr(&buf[loc].value);
		}

		ck_pr_fence_load();
		if (n <= 1 ||
		    (uint64_t)ck_pr_load_ptr(&buf[base_loc].gen) == base_cursor) {
			return n;
		}

		if (!hard) {
			return 0;
		}

		/* We started with snap.gen == last_cursor, so we lost a race. */
		n = 1;
		goto retry;
	}

slow:
	if (n == 0) {
		return 0;
	}

	for (consumed = 0; consumed < n; consumed++) {
		struct ck_pring_elt snap;
		uint64_t cursor = base_cursor + consumed;
		size_t loc = cursor & mask;

		snap.gen = ck_pr_load_ptr(&buf[loc].gen);
		ck_pr_fence_load();
		snap.value = ck_pr_load_ptr(&buf[loc].value);
		if ((uint64_t)snap.gen != cursor) {
			break;
		}

		dst[consumed] = snap.value;
	}

	if (consumed == 0 && hard) {
		uint64_t gen;

		gen = (uint64_t)ck_pr_load_ptr(&buf[base_loc].gen);
		/* Only retry if we lost the race. */
		if ((int64_t)(gen - base_cursor) > 0) {
			n = 1;
			goto retry;
		}
	}

	return consumed;
}

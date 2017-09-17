#include <assert.h>
#include <ck_pring/snoop.h>

#include <sys/types.h>

static bool
snoop_update_cursor(struct ck_pring_snooper *snoop,
    const struct ck_pring *ring, bool init)
{
	const struct ck_pring_elt *buf = snoop->cons.buf;
	uint64_t mask = snoop->cons.mask;
	uint64_t cursor = snoop->cons.cursor;
	uint64_t new_cursor;
	size_t loc = cursor & mask;

	if (snoop->cons.dependency_begin < snoop->cons.dependency_end) {
		(void)ck_pring_consumer_update_limit(&snoop->cons, ring);
		new_cursor = snoop->cons.read_limit - 1;
	} else {
		new_cursor = (uint64_t)ck_pr_load_ptr(&buf[loc].gen);
	}

	if (!init && (int64_t)(cursor - new_cursor) >= 0) {
		return false;
	}

	snoop->cons.cursor = new_cursor;
	return true;
}

void
ck_pring_snoop_init(struct ck_pring_snooper *snoop, const struct ck_pring *ring,
    uint32_t dep_begin, uint32_t dep_end)
{

	snoop->cons.buf = ring->prod.buf;
	snoop->cons.mask = ring->prod.mask;
	snoop->cons.dependency_begin = dep_begin;
	snoop->cons.dependency_end = dep_end;
	(void)snoop_update_cursor(snoop, ring, true);
	return;
}

uintptr_t
ck_pring_snoop_slow(struct ck_pring_snooper *snoop,
    const struct ck_pring *ring)
{
	uintptr_t ret[1] = { 0 };
	uintptr_t n;

	n = ck_pring_snoop_n(snoop, ring, ret, 1);
	/* return 0 if n == 0, otherwise ret (and n == 1). */
	return ret[0] & -n;
}

static ssize_t
ck_pring_snoop_n_inner(struct ck_pring_snooper *snoop, uintptr_t *dst, size_t n)
{
	struct ck_pring_consumer *cons = &snoop->cons;
	const struct ck_pring_elt *buf = cons->buf;
	uint64_t mask = cons->mask;
	uint64_t base_cursor = cons->cursor; /* only writer is us. */
	uint64_t base_gen;
	size_t base_loc = base_cursor & mask;
	size_t consumed;

	base_gen = (uint64_t)ck_pr_load_ptr(&buf[base_loc].gen);
	if ((int64_t)(base_gen - base_cursor) < 0) {
		/* the queue is empty. */
		return 0;
	}

	for (consumed = 0; consumed < n; consumed++) {
		uint64_t cursor = base_cursor + consumed;
		uint64_t gen;
		size_t loc = cursor & mask;

		gen = (uint64_t)ck_pr_load_ptr(&buf[loc].gen);
		ck_pr_fence_load();
		dst[consumed] = ck_pr_load_ptr(&buf[loc].value);
		if (gen != cursor) {
			break;
		}
	}

	ck_pr_fence_load();
	/* everything matched up to here. make sure we didn't lose the race. */
	base_gen = (uint64_t)ck_pr_load_ptr(&buf[base_loc].gen);
	return (base_gen == base_cursor) ? (ssize_t)consumed : -1;
}

size_t
ck_pring_snoop_n(struct ck_pring_snooper *snoop, const struct ck_pring *ring,
    uintptr_t *dst, size_t n)
{
	ssize_t ret;

	if (n == 0) {
		return 0;
	}

	if (n > snoop->cons.mask) {
		n = snoop->cons.mask + 1;
	}

	for (;;) {
		struct ck_pring_consumer *cons = &snoop->cons;
		uint64_t cursor = cons->cursor;
		uint64_t read_limit = cons->read_limit;
		size_t remaining = read_limit - cursor;

		if (CK_CC_UNLIKELY((int64_t)(cursor - read_limit) >= 0)) {
			remaining = ck_pring_consumer_update_limit(cons, ring);
			if (remaining == 0) {
				return remaining;
			}
		}

		n = (n > remaining) ? remaining : n;
		ret = ck_pring_snoop_n_inner(snoop, dst, n);
		if (ret >= 0) {
			break;
		}

		n = (n + 1) / 2;
		if (!snoop_update_cursor(snoop, ring, false)) {
			ret = 0;
			break;
		}
	}

	snoop->cons.cursor = snoop->cons.cursor + (size_t)ret;
	return (size_t)ret;
}

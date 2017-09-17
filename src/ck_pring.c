#define CK_PRING_IMPL
#include <assert.h>
#include <ck_pring.h>

#include <string.h>

size_t
ck_pring_allocation_size(size_t n_consumer)
{
	size_t consumer_size;
	size_t extra = 0;

	if (n_consumer > 0) {
		extra = n_consumer - 1;
	}

	consumer_size = extra * sizeof(struct ck_pring_consumer_block);
	return sizeof(struct ck_pring) + consumer_size;
}

void
ck_pring_init(struct ck_pring *ring, size_t n_consumer,
    struct ck_pring_elt *buf, size_t size)
{
	struct ck_pring_consumer_block *next = (void *)(ring + 1);

	assert(size > 0 && (size & (size - 1)) == 0);
	*ring = (struct ck_pring) CK_PRING_INIT_(buf, size, n_consumer);
	for (size_t i = 1; i < n_consumer; i++) {
		next[i - 1].cons = ring->cons.cons;
	}

	ck_pr_fence_store();
	return;
}

struct ck_pring *
ck_pring_create(struct ck_malloc *ck_malloc,
    size_t n_consumer, struct ck_pring_elt *buf, size_t bufsz)
{
	struct ck_pring *ret;
	size_t ringsz;

	ringsz = ck_pring_allocation_size(n_consumer);
	ret = ck_malloc->malloc(ringsz);
	memset(ret, 0, ringsz);
	ck_pring_init(ret, n_consumer, buf, bufsz);
	return ret;
}

void
ck_pring_destroy(struct ck_malloc *ck_malloc, struct ck_pring *ring)
{
	size_t ringsz;

	if (ring == NULL) {
		return;
	}

	ringsz = ck_pring_allocation_size(ring->prod.n_consumer);
	ck_malloc->free(ring, ringsz, false);
	return;
}

struct ck_pring_elt *
ck_pring_buffer(const struct ck_pring *ring)
{

	return ring->prod.buf;
}

/* Declared in pring_common.h */
uintptr_t
ck_pring_consumer_update_limit(struct ck_pring_consumer *consumer,
    const struct ck_pring *ring)
{
	const struct ck_pring_consumer_block *consumers = &ring->cons;
	uint64_t old_limit = ck_pr_load_64(&consumer->read_limit);
	uint64_t limit = old_limit + (1UL << 60);
	uint64_t capacity;
	size_t dep_begin = consumer->dependency_begin;
	size_t dep_end = consumer->dependency_end;

	/* Common case: no dependency. */
	if (CK_CC_LIKELY(dep_begin >= dep_end)) {
		capacity = consumer->mask + 1;
		ck_pr_store_64(&consumer->read_limit, limit);
		return capacity;
	}

	for (size_t i = dep_end; i --> dep_begin; ) {
		const struct ck_pring_consumer *current = &consumers[i].cons;
		uint64_t current_cursor;
		size_t begin = current->dependency_begin;
		size_t skip;

		current_cursor = ck_pr_load_64(&current->cursor);
		skip = (current->dependency_end >= i) ? begin : i;
		if ((int64_t)(current_cursor - limit) < 0) {
			limit = current_cursor;
		}

		i = (skip < i) ? skip : i;
	}

	capacity = limit - ck_pr_load_64(&consumer->cursor);
	ck_pr_store_64(&consumer->read_limit, limit);
	return ((int64_t)capacity > 0) ? capacity : 0;
}

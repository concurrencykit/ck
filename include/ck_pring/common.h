#ifndef PRING_COMMON_H
#define PRING_COMMON_H
#include <ck_cc.h>
#include <ck_md.h>
#include <ck_pr.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct ck_pring_elt {
	char *gen;
	uintptr_t value;
} CK_CC_ALIGN(2 * sizeof(void *));

/**
 * State for producers.
 *
 * consumer_snap is a lower bound on the oldest consumer cursor.
 * We must not write past consumer_snap + mask.  It is monotonic
 * in the SP case, best effort otherwise.
 *
 * cursor is a lower bound on the next empty element.  It is exact
 * (and thus monotonic) in the single-producer case, best effort for
 * MP.
 */
struct ck_pring_producer {
	uint64_t consumer_snap; /* <= cons.cursor, accessed by prods. */
	uint64_t cursor; /* <= real prod.cursor, accessed by prods & snoops. */
	struct ck_pring_elt *buf; /* write once */
	uint64_t mask; /* write once */
	size_t n_consumer;
	uintptr_t dummy;
};

/**
 * State for consumers.
 *
 * cursor is exact (and thus monotonic), the index for the next value
 * we wish to read.  any sequence < cursor is safe to overwrite (which
 * lets writers go up to (cursor - 1 + size) = cursor + mask).
 *
 * read_limit is a lower bound on the exclusive range that is safe to
 * read (i.e., we can always read non-empty sequence < read_limit).
 * Normal consumers, without consumer dependencies, mostly ignore this
 * field (it is set to arbitrarily high values).  Consumers with
 * dependencies use this field to make sure they never overtake their
 * parents: at any time, for any parent, read_limit <= parent.cursor.
 * read_limit is monotonic for SC, best effort for MC.
 *
 * dependency_begin, end mark the half-open range of this consumer's
 * parents.
 */
struct ck_pring_consumer {
	const struct ck_pring_elt *buf; /* write-once. */
	uint64_t mask; /* write-once. */
	uint64_t cursor; /* written by consumers, read by all. */
	uint64_t read_limit; /* private. cursor < read_limit. */
	uint32_t dependency_begin; /* write once */
	uint32_t dependency_end; /* write once */
};

/**
 * A snooper is a consumer hidden from the rest of the system.
 * Instead of relying on cursor to protect reads, snoopers must check
 * the generation counter on ring buffer elements.
 */
struct ck_pring_snooper {
	struct ck_pring_consumer cons;
};

struct ck_pring_consumer_block {
	struct ck_pring_consumer cons;
	char padding[CK_MD_CACHELINE - sizeof(struct ck_pring_consumer)];
} CK_CC_CACHELINE;

/**
 * Pack more consumer blocks immediately after ck_pring for
 * multi-consumer allocations.
 */
struct ck_pring {
	struct ck_pring_producer prod;
	char padding[CK_MD_CACHELINE - sizeof(struct ck_pring_producer)];
	struct ck_pring_consumer_block cons;
} CK_CC_CACHELINE;

#define CK_PRING_INIT(BUF)						\
	CK_PRING_INIT_((BUF),						\
	    CK_PRING_PO2_(sizeof(BUF) / sizeof(struct ck_pring_elt)),	\
	    1)

/**
 * Inline internals.
 */

/**
 * Return the data block for consumer index (< n_consumer).
 */
static CK_CC_FORCE_INLINE struct ck_pring_consumer *
ck_pring_consumer_by_id(struct ck_pring *ring, size_t index)
{
	struct ck_pring_consumer_block *consumers = &ring->cons;

	/* index should only be > 0 with a heap-allocated tail of consumers. */
	return &consumers[index].cons;
}

/**
 * Update the read limit for the consumer.  If there are no
 * dependencies, arranges updates to make sure it's called very
 * rarely.
 *
 * Return the new capacity if it is strictly positive, 0 otherwise.
 */
uintptr_t
ck_pring_consumer_update_limit(struct ck_pring_consumer *,
    const struct ck_pring *);

#define CK_PRING_INIT_(BUF, SIZE, N_CONSUMER)			\
	{							\
		.prod = {					\
			.consumer_snap = (SIZE),		\
			.cursor = (SIZE),			\
			.buf = (BUF),				\
			.mask = (SIZE) - 1,			\
			.n_consumer = (N_CONSUMER)		\
		},						\
		.cons = {					\
			.cons = {				\
				.buf = (BUF),			\
				.mask = (SIZE) - 1,		\
				.cursor = (SIZE)		\
			}					\
		}						\
	}

#define CK_PRING_PO2_(X)						\
	((X) * sizeof(char[2 * !!((X) > 0 && ((X) & ((X) - 1)) == 0) - 1]))
#endif /* !PRING_COMMON_H */

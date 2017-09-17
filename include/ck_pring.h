#ifndef CK_PRING_H
#define CK_PRING_H
#include <ck_malloc.h>
#include <ck_pring/common.h>
#include <ck_pring/dequeue.h>
#include <ck_pring/enqueue.h>
#include <ck_pring/snoop.h>

/**
 * Lock-free *P*C disruptor for pointer-sized values.
 *
 * The consumer and producer sides may be independently specialized to
 * the single producer/consumer case.
 *
 * On my 2.2 GHz Haswell laptop, the best-case inverse throughput for:
 *   - SP is ~4 cycles/enqueue;
 *   - SC is ~5 cycles/dequeue;
 *   - MP is ~24 cycles/enqueue;
 *   - MC is ~27 cycles/dequeue.
 *
 * Consumption is slightly slower than production, but consumption can
 * be batched.
 *
 * The ring buffer for the pointer-sized values is an array of
 * struct ck_pring_elt, pairs of:
 *  - generation counter;
 *  - value.
 *
 * The generation counter prevents ABA: when a producer is slow, it's
 * hard to tell if a ring buffer element is *still* empty, or *newly*
 * empty.
 *
 * This ring buffer attempts to minimise the communication (cache
 * coherency traffic) between classes of threads (producers and
 * consumers).  Producers access the `prod` field, read/write to the
 * ring buffer, and only read the `cons` field to refresh their cache
 * of the consumer(s) cursor.  Consumers access the `cons` field, and
 * read the ring buffer; they *never* access the `prod` field.
 *
 * Production thus mostly incurs traffic for communications between
 * producers (hard to avoid), any compulsory traffic on the ring
 * buffer (if the buffer is large enough, this is only between
 * producers).  Producers only interact with consumers when their
 * snapshot of the consumer cursor hints that the buffer might be
 * full.  If consumers keep up with producers, this should only happen
 * ~once per buffer_size enqueue.
 *
 * Consumption only incurs traffic for communications between
 * consumers (racing on the consumption cursor), and any compulsory
 * traffic to read the ring buffer (negligible with a large enough
 * buffer).
 *
 * Producers race along an unbounded sequence of generation counters
 * (64 bit pointers) to atomically acquire empty cells and fill them
 * with their value.  The actual storage for this unbounded sequence
 * is the ring buffer: the cell for sequence id x is `x % ring_size`.
 *
 * The cell is available if its generation value is < the generation
 * counter we wish to acquire.  The cell must also have been released
 * by the consumer(s).  The producer struct has a cache of the
 * consumer(s)'s cursor; any generation value strictly less than that
 * cursor is fair game.  Of course, the cache may be stale (but must
 * never be ahead of the actual value); when producers notice that the
 * cached consumer cursor is too low, they update it before exiting
 * with failure (full ring).
 *
 * The linear search over an unbounded sequence of generation counter
 * isn't practical.  Producers maintain an approximate cache of the
 * next free generation counter: when a producer succeeds at producing
 * a value for sequence counter `gen`, it (racily) stores `gen + 1` in
 * prod.cursor.  Producers thus begin their search at `prod.cursor`.
 * That search is bounded: it can't go further than the consumer's
 * cursor + ring_size - 1.  Producers will only retry if the
 * consumer's cursor has moved ahead, in which case the system has
 * made global progress.
 *
 * Concurrent production uses a double-wide CAS (CMPXCHG16B) to
 * atomically acquire/update the generation counter and set the value
 * (we only need single compare / double-wide set), which trivially
 * guarantees visibility (on TSO).  For the SP case, we must be
 * careful to write the value before publishing it by updating the
 * generation.
 *
 * Consuming is easier.  The consumer(s) maintain a cursor for the
 * next sequence value to consume.  The storage for that sequence
 * is always at `cursor % ring_size`.  They simply have to wait
 * until that cell is populated with the correct sequence value,
 * and update/race on cons.cursor.
 *
 * The value in cons.cursor imposes a limit on the largest generation
 * that could be produced, so it's safe to:
 *  1. read the generation;
 *  2. read the value;
 *  3. update cons.cursor.
 *
 * If the generation matches at step 1, it must also match at step 2,
 * since it can only increase after step 3.  This assumes fences
 * between each step... fences that are happily no-ops on TSO.
 */

/**
 * Size in bytes for a pring with n_consumer consumers.
 */
size_t
ck_pring_allocation_size(size_t n_consumer);

/**
 * Initialize a pring of n_consumer
 */
void
ck_pring_init(struct ck_pring *, size_t n_consumer, struct ck_pring_elt *, size_t);

/**
 * Allocate a pring for n consumers, with buf of bufsz elements.
 *
 * If consumers have dependencies, the caller is expected to set up
 * the dependency begin/end half-open ranges: a consumer block with
 * dependency [begin, end) will only process items after they have
 * been consumed by consumer ids [begin, end).
 */
struct ck_pring *
ck_pring_create(struct ck_malloc *,
    size_t n_consumer, struct ck_pring_elt *buf, size_t bufsz);

/**
 * Deallocate the pring *but not the buffer*.
 */
void
ck_pring_destroy(struct ck_malloc *, struct ck_pring *);

static inline size_t
ck_pring_size(const struct ck_pring *);

struct ck_pring_elt *
ck_pring_buffer(const struct ck_pring *);

/**
 * Inline implementation.
 */
static inline size_t
ck_pring_size(const struct ck_pring *ring)
{

	return 1 + ring->prod.mask;
}
#endif /* !CK_PRING_H */

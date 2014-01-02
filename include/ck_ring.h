/*
 * Copyright 2009-2014 Samy Al Bahra.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _CK_RING_H
#define _CK_RING_H

#include <ck_cc.h>
#include <ck_md.h>
#include <ck_pr.h>
#include <stdbool.h>
#include <string.h>

/*
 * Concurrent ring buffer.
 */

struct ck_ring {
	unsigned int c_head;
	char pad[CK_MD_CACHELINE - sizeof(unsigned int)];
	unsigned int p_tail;
	char _pad[CK_MD_CACHELINE - sizeof(unsigned int)];
	unsigned int size;
	unsigned int mask;
};
typedef struct ck_ring ck_ring_t;

struct ck_ring_buffer {
	void *value;
};
typedef struct ck_ring_buffer ck_ring_buffer_t;

CK_CC_INLINE static unsigned int
ck_ring_size(struct ck_ring *ring)
{
	unsigned int c, p;

	c = ck_pr_load_uint(&ring->c_head);
	p = ck_pr_load_uint(&ring->p_tail);
	return (p - c) & ring->mask;
}

CK_CC_INLINE static unsigned int
ck_ring_capacity(struct ck_ring *ring)
{

	return ring->size;
}

/*
 * Atomically enqueues the specified entry. Returns true on success, returns
 * false if the ck_ring is full. This operation only support one active
 * invocation at a time and works in the presence of a concurrent invocation
 * of ck_ring_dequeue_spsc.
 *
 * This variant of ck_ring_enqueue_spsc returns the snapshot of queue length
 * with respect to the linearization point. This can be used to extract ring
 * size without incurring additional cacheline invalidation overhead from the
 * writer.
 */
CK_CC_INLINE static bool
_ck_ring_enqueue_spsc_size(struct ck_ring *ring,
    void *restrict buffer,
    const void *restrict entry,
    unsigned int type_size,
    unsigned int *size)
{
	unsigned int consumer, producer, delta;
	unsigned int mask = ring->mask;

	consumer = ck_pr_load_uint(&ring->c_head);
	producer = ring->p_tail;
	delta = producer + 1;
	*size = (producer - consumer) & mask;

	if ((delta & mask) == (consumer & mask))
		return false;

	buffer = (char *)buffer + type_size * (producer & mask);
	memcpy(buffer, entry, type_size);

	/*
	 * Make sure to update slot value before indicating
	 * that the slot is available for consumption.
	 */
	ck_pr_fence_store();
	ck_pr_store_uint(&ring->p_tail, delta);
	return true;
}

CK_CC_INLINE static bool
ck_ring_enqueue_spsc_size(struct ck_ring *ring,
    struct ck_ring_buffer *buffer,
    void *entry,
    unsigned int *size)
{

	return _ck_ring_enqueue_spsc_size(ring, buffer, &entry,
	    sizeof(void *), size);
}

/*
 * Atomically enqueues the specified entry. Returns true on success, returns
 * false if the ck_ring is full. This operation only support one active
 * invocation at a time and works in the presence of a concurrent invocation
 * of ck_ring_dequeue_spsc.
 */
CK_CC_INLINE static bool
_ck_ring_enqueue_spsc(struct ck_ring *ring,
    void *restrict destination,
    const void *restrict source,
    unsigned int size)
{
	unsigned int consumer, producer, delta;
	unsigned int mask = ring->mask;

	consumer = ck_pr_load_uint(&ring->c_head);
	producer = ring->p_tail;
	delta = producer + 1;

	if ((delta & mask) == (consumer & mask))
		return false;

	destination = (char *)destination + size * (producer & mask);
	memcpy(destination, source, size);

	/*
	 * Make sure to update slot value before indicating
	 * that the slot is available for consumption.
	 */
	ck_pr_fence_store();
	ck_pr_store_uint(&ring->p_tail, delta);
	return true;
}

CK_CC_INLINE static bool
ck_ring_enqueue_spsc(struct ck_ring *ring,
    struct ck_ring_buffer *buffer,
    const void *entry)
{

	return _ck_ring_enqueue_spsc(ring, buffer,
	    &entry, sizeof(entry));
}

/*
 * Single consumer and single producer ring buffer dequeue (consumer).
 */
CK_CC_INLINE static bool
_ck_ring_dequeue_spsc(struct ck_ring *ring,
    void *restrict buffer,
    void *restrict target,
    unsigned int size)
{
	unsigned int consumer, producer;
	unsigned int mask = ring->mask;

	consumer = ring->c_head;
	producer = ck_pr_load_uint(&ring->p_tail);

	if (consumer == producer)
		return false;

	/*
	 * Make sure to serialize with respect to our snapshot
	 * of the producer counter.
	 */
	ck_pr_fence_load();

	buffer = (char *)buffer + size * (consumer & mask);
	memcpy(target, buffer, size);

	/*
	 * Make sure copy is completed with respect to consumer
	 * update.
	 */
	ck_pr_fence_store();
	ck_pr_store_uint(&ring->c_head, consumer + 1);
	return true;
}

CK_CC_INLINE static bool
ck_ring_dequeue_spsc(struct ck_ring *ring,
    struct ck_ring_buffer *buffer,
    void *data)
{

	return _ck_ring_dequeue_spsc(ring, buffer,
	    data, sizeof(void *));
}

/*
 * Atomically enqueues the specified entry. Returns true on success, returns
 * false if the ck_ring is full. This operation only support one active
 * invocation at a time and works in the presence of up to UINT_MAX concurrent
 * invocations of ck_ring_dequeue_spmc.
 *
 * This variant of ck_ring_enqueue_spmc returns the snapshot of queue length
 * with respect to the linearization point. This can be used to extract ring
 * size without incurring additional cacheline invalidation overhead from the
 * writer.
 */
CK_CC_INLINE static bool
ck_ring_enqueue_spmc_size(struct ck_ring *ring,
    struct ck_ring_buffer *buffer,
    void *entry,
    unsigned int *size)
{

	return ck_ring_enqueue_spsc_size(ring, buffer,
	    entry, size);
}

/*
 * Atomically enqueues the specified entry. Returns true on success, returns
 * false if the ck_ring is full. This operation only support one active
 * invocation at a time and works in the presence of up to UINT_MAX concurrent
 * invocations of ck_ring_dequeue_spmc.
 */
CK_CC_INLINE static bool
ck_ring_enqueue_spmc(struct ck_ring *ring,
    struct ck_ring_buffer *buffer,
    void *entry)
{

	return ck_ring_enqueue_spsc(ring, buffer, entry);
}

CK_CC_INLINE static bool
_ck_ring_trydequeue_spmc(struct ck_ring *ring,
    void *restrict buffer,
    void *data,
    unsigned int size)
{
	unsigned int consumer, producer;
	unsigned int mask = ring->mask;

	consumer = ck_pr_load_uint(&ring->c_head);
	ck_pr_fence_load();
	producer = ck_pr_load_uint(&ring->p_tail);

	if (consumer == producer)
		return false;

	ck_pr_fence_load();

	buffer = (char *)buffer + size * (consumer & mask);
	memcpy(data, buffer, size);

	ck_pr_fence_store_atomic();
	return ck_pr_cas_uint(&ring->c_head, consumer, consumer + 1);
}

CK_CC_INLINE static bool
ck_ring_trydequeue_spmc(struct ck_ring *ring,
    struct ck_ring_buffer *buffer,
    void *data)
{

	return _ck_ring_trydequeue_spmc(ring,
	    buffer, data, sizeof(void *));
}

CK_CC_INLINE static bool
_ck_ring_dequeue_spmc(struct ck_ring *ring,
    void *buffer,
    void *data,
    unsigned int size)
{
	unsigned int consumer, producer;
	unsigned int mask = ring->mask;
	char *target;

	consumer = ck_pr_load_uint(&ring->c_head);

	do {
		/*
		 * Producer counter must represent state relative to
		 * our latest consumer snapshot.
		 */
		ck_pr_fence_load();
		producer = ck_pr_load_uint(&ring->p_tail);

		if (consumer == producer)
			return false;

		ck_pr_fence_load();
		
		/*
		 * Both LLVM and GCC have generated code which completely
		 * ignores the semantics of the r load, despite it being
		 * sandwiched between compiler barriers. We use an atomic
		 * volatile load to force volatile semantics while allowing
		 * for r itself to remain aliased across the loop.
		 */
		target = (char *)buffer + size * (consumer & mask);
		memcpy(data, target, size);

		/* Serialize load with respect to head update. */
		ck_pr_fence_store_atomic();
	} while (ck_pr_cas_uint_value(&ring->c_head,
				      consumer,
				      consumer + 1,
				      &consumer) == false);

	return true;
}

CK_CC_INLINE static bool
ck_ring_dequeue_spmc(struct ck_ring *ring,
    struct ck_ring_buffer *buffer,
    void *data)
{

	return _ck_ring_dequeue_spmc(ring, buffer, data,
	    sizeof(void *));
}

CK_CC_INLINE static void
ck_ring_init(struct ck_ring *ring, unsigned int size)
{

	ring->size = size;
	ring->mask = size - 1;
	ring->p_tail = 0;
	ring->c_head = 0;
	return;
}

#define CK_RING_PROTOTYPE(name, type)			\
CK_CC_INLINE static bool				\
ck_ring_enqueue_spsc_size_##name(struct ck_ring *a,	\
    struct type *b,					\
    struct type *c,					\
    unsigned int *d)					\
{							\
							\
	return _ck_ring_enqueue_spsc_size(a, b, c,	\
	    sizeof(struct type), d);			\
}							\
							\
CK_CC_INLINE static bool				\
ck_ring_enqueue_spsc_##name(struct ck_ring *a,		\
    struct type *b,					\
    struct type *c)					\
{							\
							\
	return _ck_ring_enqueue_spsc(a, b, c,		\
	    sizeof(struct type));			\
}							\
							\
CK_CC_INLINE static bool				\
ck_ring_dequeue_spsc_##name(struct ck_ring *a,		\
    struct type *b,					\
    struct type *c)					\
{							\
							\
	return _ck_ring_dequeue_spsc(a, b, c,		\
	    sizeof(struct type));			\
}							\
							\
CK_CC_INLINE static bool				\
ck_ring_enqueue_spmc_size_##name(struct ck_ring *a,	\
    struct type *b,					\
    struct type *c,					\
    unsigned int *d)					\
{							\
							\
	return _ck_ring_enqueue_spsc_size(a, b, c,	\
	    sizeof(struct type), d);			\
}							\
							\
							\
CK_CC_INLINE static bool				\
ck_ring_enqueue_spmc_##name(struct ck_ring *a,		\
    struct type *b,					\
    struct type *c)					\
{							\
							\
	return _ck_ring_enqueue_spsc(a, b, c,		\
	    sizeof(struct type));			\
}							\
							\
CK_CC_INLINE static bool				\
ck_ring_trydequeue_spmc_##name(struct ck_ring *a,	\
    struct type *b,					\
    struct type *c)					\
{							\
							\
	return _ck_ring_trydequeue_spmc(ring,		\
	    b, c, sizeof(struct type));			\
}							\
							\
CK_CC_INLINE static bool				\
ck_ring_dequeue_spmc_##name(struct ck_ring *a,		\
    struct type *b,					\
    struct type *c)					\
{							\
							\
	return _ck_ring_dequeue_spmc(a, b, c,		\
	    sizeof(struct type));			\
}

#define CK_RING_ENQUEUE_SPSC(name, a, b, c)		\
	ck_ring_enqueue_spsc_##name(a, b, c)
#define CK_RING_ENQUEUE_SPSC_SIZE(name, a, b, c, d)	\
	ck_ring_enqueue_spsc_size_##name(a, b, c, d)
#define CK_RING_DEQUEUE_SPSC(name, a, b, c)		\
	ck_ring_dequeue_spsc_##name(a, b, c)
#define CK_RING_ENQUEUE_SPMC(name, a, b, c)		\
	ck_ring_enqueue_spmc_##name(a, b, c)
#define CK_RING_ENQUEUE_SPMC_SIZE(name, a, b, c, d)	\
	ck_ring_enqueue_spmc_size_##name(a, b, c, d)
#define CK_RING_TRYDEQUEUE_SPMC(name, a, b, c)		\
	ck_ring_trydequeue_spmc_##name(a, b, c)
#define CK_RING_DEQUEUE_SPMC(name, a, b, c)		\
	ck_ring_dequeue_spmc_##name(a, b, c)

#endif /* _CK_RING_H */


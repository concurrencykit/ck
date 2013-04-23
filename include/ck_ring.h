/*
 * Copyright 2009-2013 Samy Al Bahra.
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
#define CK_RING(type, name)							\
	struct ck_ring_##name {							\
		unsigned int c_head;						\
		char pad[CK_MD_CACHELINE - sizeof(unsigned int)];		\
		unsigned int p_tail;						\
		char _pad[CK_MD_CACHELINE - sizeof(unsigned int)];		\
		unsigned int size;						\
		unsigned int mask;						\
		struct type *ring;						\
	};									\
	CK_CC_INLINE static void						\
	ck_ring_init_##name(struct ck_ring_##name *ring,			\
			    struct type *buffer,				\
			    unsigned int size)					\
	{									\
										\
		ring->size = size;						\
		ring->mask = size - 1;						\
		ring->p_tail = 0;						\
		ring->c_head = 0;						\
		ring->ring = buffer;						\
		return;								\
	}									\
	CK_CC_INLINE static unsigned int					\
	ck_ring_size_##name(struct ck_ring_##name *ring)			\
	{									\
		unsigned int c, p;						\
										\
		c = ck_pr_load_uint(&ring->c_head);				\
		p = ck_pr_load_uint(&ring->p_tail);				\
		return (p - c) & ring->mask;					\
	}									\
	CK_CC_INLINE static unsigned int					\
	ck_ring_capacity_##name(struct ck_ring_##name *ring)			\
	{									\
										\
		return ring->size;						\
	}									\
	CK_CC_INLINE static bool						\
	ck_ring_enqueue_spsc_size_##name(struct ck_ring_##name *ring,		\
	    struct type *entry,							\
	    unsigned int *size)							\
	{									\
		unsigned int consumer, producer, delta;				\
		unsigned int mask = ring->mask;					\
										\
		consumer = ck_pr_load_uint(&ring->c_head);			\
		producer = ring->p_tail;					\
		delta = producer + 1;						\
		*size = (producer - consumer) & mask;				\
										\
		if ((delta & mask) == (consumer & mask))			\
			return false;						\
										\
		ring->ring[producer & mask] = *entry;				\
		ck_pr_fence_store();						\
		ck_pr_store_uint(&ring->p_tail, delta);				\
		return true;							\
	}									\
	CK_CC_INLINE static bool						\
	ck_ring_enqueue_spsc_##name(struct ck_ring_##name *ring,		\
				    struct type *entry)				\
	{									\
		unsigned int consumer, producer, delta;				\
		unsigned int mask = ring->mask;					\
										\
		consumer = ck_pr_load_uint(&ring->c_head);			\
		producer = ring->p_tail;					\
		delta = producer + 1;						\
										\
		if ((delta & mask) == (consumer & mask))			\
			return false;						\
										\
		ring->ring[producer & mask] = *entry;				\
		ck_pr_fence_store();						\
		ck_pr_store_uint(&ring->p_tail, delta);				\
		return true;							\
	}									\
	CK_CC_INLINE static bool 						\
	ck_ring_dequeue_spsc_##name(struct ck_ring_##name *ring,		\
				    struct type *data)				\
	{									\
		unsigned int consumer, producer;				\
		unsigned int mask = ring->mask;					\
										\
		consumer = ring->c_head;					\
		producer = ck_pr_load_uint(&ring->p_tail);			\
										\
		if (consumer == producer)					\
			return false;						\
										\
		ck_pr_fence_load();						\
		*data = ring->ring[consumer & mask];				\
		ck_pr_fence_store();						\
		ck_pr_store_uint(&ring->c_head, consumer + 1);			\
										\
		return true;							\
	}									\
	CK_CC_INLINE static bool						\
	ck_ring_enqueue_spmc_size_##name(struct ck_ring_##name *ring,		\
	    void *entry, unsigned int *size)					\
	{									\
										\
		return ck_ring_enqueue_spsc_size_##name(ring, entry, size);	\
	}									\
	CK_CC_INLINE static bool						\
	ck_ring_enqueue_spmc_##name(struct ck_ring_##name *ring, void *entry)	\
	{									\
										\
		return ck_ring_enqueue_spsc_##name(ring, entry);		\
	}									\
	CK_CC_INLINE static bool						\
	ck_ring_trydequeue_spmc_##name(struct ck_ring_##name *ring,		\
				       struct type *data)			\
	{									\
		unsigned int consumer, producer;				\
		unsigned int mask = ring->mask;					\
										\
		consumer = ck_pr_load_uint(&ring->c_head);			\
		ck_pr_fence_load();						\
		producer = ck_pr_load_uint(&ring->p_tail);			\
										\
		if (consumer == producer)					\
			return false;						\
										\
		ck_pr_fence_load();						\
		*data = ring->ring[consumer & mask];				\
		ck_pr_fence_memory();						\
		return ck_pr_cas_uint(&ring->c_head,				\
				      consumer,					\
				      consumer + 1);				\
	}									\
	CK_CC_INLINE static bool						\
	ck_ring_dequeue_spmc_##name(struct ck_ring_##name *ring,		\
				    struct type *data)				\
	{									\
		unsigned int consumer, producer;				\
		unsigned int mask = ring->mask;					\
										\
		consumer = ck_pr_load_uint(&ring->c_head);			\
		do {								\
			ck_pr_fence_load();					\
			producer = ck_pr_load_uint(&ring->p_tail);		\
										\
			if (consumer == producer)				\
				return false;					\
										\
			ck_pr_fence_load();					\
			*data = ring->ring[consumer & mask];			\
			ck_pr_fence_memory();					\
		} while (ck_pr_cas_uint_value(&ring->c_head,			\
					      consumer,				\
					      consumer + 1,			\
					      &consumer) == false);		\
										\
		return true;							\
	}


#define CK_RING_INSTANCE(name)					\
	struct ck_ring_##name
#define CK_RING_INIT(name, object, buffer, size)		\
	ck_ring_init_##name(object, buffer, size)
#define CK_RING_SIZE(name, object)				\
	ck_ring_size_##name(object)
#define CK_RING_CAPACITY(name, object)				\
	ck_ring_capacity_##name(object)
#define CK_RING_ENQUEUE_SPSC_SIZE(name, object, value, s)	\
	ck_ring_enqueue_spsc_size_##name(object, value, s)
#define CK_RING_ENQUEUE_SPSC(name, object, value)		\
	ck_ring_enqueue_spsc_##name(object, value)
#define CK_RING_DEQUEUE_SPSC(name, object, value)		\
	ck_ring_dequeue_spsc_##name(object, value)
#define CK_RING_DEQUEUE_SPMC(name, object, value)		\
	ck_ring_dequeue_spmc_##name(object, value)
#define CK_RING_TRYDEQUEUE_SPMC(name, object, value)		\
	ck_ring_trydequeue_spmc_##name(object, value)
#define CK_RING_ENQUEUE_SPMC_SIZE(name, object, value, s)	\
	ck_ring_enqueue_spmc_size_##name(object, value, s)
#define CK_RING_ENQUEUE_SPMC(name, object, value)		\
	ck_ring_enqueue_spmc_##name(object, value)

struct ck_ring {
	unsigned int c_head;
	char pad[CK_MD_CACHELINE - sizeof(unsigned int)];
	unsigned int p_tail;
	char _pad[CK_MD_CACHELINE - sizeof(unsigned int)];
	unsigned int size;
	unsigned int mask;
	void **ring;
};
typedef struct ck_ring ck_ring_t;

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
ck_ring_enqueue_spsc_size(struct ck_ring *ring,
    void *entry,
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

	ring->ring[producer & mask] = entry;

	/*
	 * Make sure to update slot value before indicating
	 * that the slot is available for consumption.
	 */
	ck_pr_fence_store();
	ck_pr_store_uint(&ring->p_tail, delta);
	return true;
}

/*
 * Atomically enqueues the specified entry. Returns true on success, returns
 * false if the ck_ring is full. This operation only support one active
 * invocation at a time and works in the presence of a concurrent invocation
 * of ck_ring_dequeue_spsc.
 */
CK_CC_INLINE static bool
ck_ring_enqueue_spsc(struct ck_ring *ring, void *entry)
{
	unsigned int consumer, producer, delta;
	unsigned int mask = ring->mask;

	consumer = ck_pr_load_uint(&ring->c_head);
	producer = ring->p_tail;
	delta = producer + 1;

	if ((delta & mask) == (consumer & mask))
		return false;

	ring->ring[producer & mask] = entry;

	/*
	 * Make sure to update slot value before indicating
	 * that the slot is available for consumption.
	 */
	ck_pr_fence_store();
	ck_pr_store_uint(&ring->p_tail, delta);
	return true;
}

/*
 * Single consumer and single producer ring buffer dequeue (consumer).
 */
CK_CC_INLINE static bool
ck_ring_dequeue_spsc(struct ck_ring *ring, void *data)
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

	/*
	 * This is used to work-around aliasing issues (C
	 * lacks a generic pointer to pointer despite it
	 * being a reality on POSIX). This interface is
	 * troublesome on platforms where sizeof(void *)
	 * is not guaranteed to be sizeof(T *).
	 */
	ck_pr_store_ptr(data, ring->ring[consumer & mask]);
	ck_pr_fence_store();
	ck_pr_store_uint(&ring->c_head, consumer + 1);
	return true;
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
    void *entry,
    unsigned int *size)
{

	return ck_ring_enqueue_spsc_size(ring, entry, size);
}

/*
 * Atomically enqueues the specified entry. Returns true on success, returns
 * false if the ck_ring is full. This operation only support one active
 * invocation at a time and works in the presence of up to UINT_MAX concurrent
 * invocations of ck_ring_dequeue_spmc.
 */
CK_CC_INLINE static bool
ck_ring_enqueue_spmc(struct ck_ring *ring, void *entry)
{

	return ck_ring_enqueue_spsc(ring, entry);
}

CK_CC_INLINE static bool
ck_ring_trydequeue_spmc(struct ck_ring *ring, void *data)
{
	unsigned int consumer, producer;
	unsigned int mask = ring->mask;

	consumer = ck_pr_load_uint(&ring->c_head);
	ck_pr_fence_load();
	producer = ck_pr_load_uint(&ring->p_tail);

	if (consumer == producer)
		return false;

	ck_pr_fence_load();
	ck_pr_store_ptr(data, ring->ring[consumer & mask]);
	ck_pr_fence_memory();

	return ck_pr_cas_uint(&ring->c_head, consumer, consumer + 1);
}

CK_CC_INLINE static bool
ck_ring_dequeue_spmc(struct ck_ring *ring, void *data)
{
	unsigned int consumer, producer;
	unsigned int mask = ring->mask;
	void *r;

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
		r = ck_pr_load_ptr(&ring->ring[consumer & mask]);

		/* Serialize load with respect to head update. */
		ck_pr_fence_memory();
	} while (ck_pr_cas_uint_value(&ring->c_head,
				      consumer,
				      consumer + 1,
				      &consumer) == false);

	/*
	 * Force spillage while avoiding aliasing issues that aren't
	 * a problem on POSIX.
	 */
	ck_pr_store_ptr(data, r);
	return true;
}

CK_CC_INLINE static void
ck_ring_init(struct ck_ring *ring, void *buffer, unsigned int size)
{

	memset(buffer, 0, sizeof(void *) * size);
	ring->size = size;
	ring->mask = size - 1;
	ring->p_tail = 0;
	ring->c_head = 0;
	ring->ring = buffer;
	return;
}

#endif /* _CK_RING_H */

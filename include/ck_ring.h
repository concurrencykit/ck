/*
 * Copyright 2009-2012 Samy Al Bahra.
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

#ifndef CK_F_RING
#define CK_F_RING

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
		ck_pr_store_uint(&ring->size, size);				\
		ck_pr_store_uint(&ring->mask, size - 1);			\
		ck_pr_store_uint(&ring->p_tail, 0);				\
		ck_pr_store_uint(&ring->c_head, 0);				\
		ck_pr_store_ptr(&ring->ring, buffer);				\
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
		return ck_pr_load_uint(&ring->size);				\
	}									\
	CK_CC_INLINE static bool						\
	ck_ring_enqueue_spsc_##name(struct ck_ring_##name *ring,		\
				    struct type *entry)				\
	{									\
		unsigned int consumer, producer, size;				\
										\
		consumer = ck_pr_load_uint(&ring->c_head);			\
		producer = ck_pr_load_uint(&ring->p_tail);			\
		size = ck_pr_load_uint(&ring->size);				\
										\
		if (((producer + 1) & ring->mask) == consumer) 			\
			return (false);						\
										\
		ring->ring[producer] = *entry;					\
		ck_pr_fence_store();						\
		ck_pr_store_uint(&ring->p_tail,	(producer + 1) & ring->mask);	\
										\
		return (true);							\
	}									\
	CK_CC_INLINE static bool 						\
	ck_ring_dequeue_spsc_##name(struct ck_ring_##name *ring,		\
				    struct type *data)				\
	{									\
		unsigned int consumer, producer;				\
										\
		consumer = ck_pr_load_uint(&ring->c_head);			\
		producer = ck_pr_load_uint(&ring->p_tail);			\
										\
		if (consumer == producer)					\
			return (false);						\
										\
		ck_pr_fence_load();						\
		*data = ring->ring[consumer];					\
		ck_pr_store_uint(&ring->c_head, (consumer + 1) & ring->mask);	\
										\
		return (true);							\
	}


#define CK_RING_INSTANCE(name)				\
	struct ck_ring_##name
#define CK_RING_INIT(name, object, buffer, size)	\
	ck_ring_init_##name(object, buffer, size)
#define CK_RING_SIZE(name, object)			\
	ck_ring_size_##name(object)
#define CK_RING_CAPACITY(name, object)			\
	ck_ring_capacity_##name(object)
#define CK_RING_ENQUEUE_SPSC(name, object, value)	\
	ck_ring_enqueue_spsc_##name(object, value)
#define CK_RING_DEQUEUE_SPSC(name, object, value)	\
	ck_ring_dequeue_spsc_##name(object, value)

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

/*
 * Single consumer and single producer ring buffer enqueue (producer).
 */
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

	return ck_pr_load_uint(&ring->size);
}

CK_CC_INLINE static bool
ck_ring_enqueue_spsc(struct ck_ring *ring, void *entry)
{
	unsigned int consumer, producer, size;

	consumer = ck_pr_load_uint(&ring->c_head);
	producer = ck_pr_load_uint(&ring->p_tail);
	size = ck_pr_load_uint(&ring->size);

	if (((producer + 1) & ring->mask) == consumer)
		return (false);

	ck_pr_store_ptr(&ring->ring[producer], entry);
	ck_pr_fence_store();
	ck_pr_store_uint(&ring->p_tail, (producer + 1) & ring->mask);

	return (true);
}

/*
 * Single consumer and single producer ring buffer dequeue (consumer).
 */
CK_CC_INLINE static bool
ck_ring_dequeue_spsc(struct ck_ring *ring, void *data)
{
	unsigned int consumer, producer;
	void *value;

	consumer = ck_pr_load_uint(&ring->c_head);
	producer = ck_pr_load_uint(&ring->p_tail);

	if (consumer == producer)
		return (false);

	ck_pr_fence_load();
	value = ck_pr_load_ptr(ring->ring + consumer);
	ck_pr_store_ptr(data, value);
	ck_pr_store_uint(&ring->c_head, (consumer + 1) & ring->mask);

	return (true);
}

CK_CC_INLINE static void
ck_ring_init(struct ck_ring *ring, void *buffer, unsigned int size)
{

	ck_pr_store_uint(&ring->size, size);
	ck_pr_store_uint(&ring->mask, size - 1);
	ck_pr_store_uint(&ring->p_tail, 0);
	ck_pr_store_uint(&ring->c_head, 0);
	ck_pr_store_ptr(&ring->ring, buffer);
	return;
}
#endif /* CK_F_RING */

#endif /* _CK_RING_H */

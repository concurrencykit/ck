/*
 * Copyright 2026 Samy Al Bahra.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyrights
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyrights
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

#include <ck_hs.h>

#include <ck_malloc.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../common.h"
#include "../../../src/ck_hs.c"

static void *
hs_malloc(size_t r)
{

	return malloc(r);
}

static void
hs_free(void *p, size_t b, bool r)
{

	(void)b;
	(void)r;
	free(p);
	return;
}

static struct ck_malloc my_allocator = {
	.malloc = hs_malloc,
	.free = hs_free
};

static unsigned long
hs_hash(const void *object, unsigned long seed)
{

	(void)object;
	(void)seed;
	return 0;
}

static void
test(unsigned int mode, enum ck_hs_probe_behavior behavior)
{
	static int a, b, c, d;
	const void **slot, **first, *object;
	unsigned long n_probes;
	ck_hs_t hs;

	if (ck_hs_init(&hs, mode | CK_HS_MODE_SPMC | CK_HS_MODE_DIRECT, hs_hash,
	    NULL, &my_allocator, 64, 0) == false)
		ck_error("ck_hs_init\n");

	/* Three keys at probe positions 1, 2 and 3, then the last two removed. */
	if (ck_hs_put(&hs, 0, &a) == false || ck_hs_put(&hs, 0, &b) == false ||
	    ck_hs_put(&hs, 0, &c) == false)
		ck_error("ck_hs_put\n");

	if (ck_hs_remove(&hs, 0, &b) != &b || ck_hs_remove(&hs, 0, &c) != &c)
		ck_error("ck_hs_remove\n");

	/* A full pass brings the bucket's bound back to the one live key. */
	if (ck_hs_gc(&hs, 0, 0) == false)
		ck_error("ck_hs_gc\n");

	if (ck_hs_map_bound_get(hs.map, 0) != 1)
		ck_error("bound is %u, expected 1\n", ck_hs_map_bound_get(hs.map, 0));

	/*
	 * An absent key passes the bound after the live key and meets the
	 * first tombstone at position 2, where the walk ends.
	 */
	slot = ck_hs_map_probe(&hs, hs.map, &n_probes, &first, 0, &d, &object,
	    hs.map->probe_limit, behavior);

	if (object != NULL)
		ck_error("absent key matched\n");

	if (first == NULL || *first != CK_HS_TOMBSTONE)
		ck_error("tombstone not offered as the priority slot\n");

	if (n_probes != 2)
		ck_error("walk ended at position %lu, expected 2\n", n_probes);

	if (slot != NULL)
		ck_error("no match, but a slot was returned\n");

	/* Through the public interface the key lands on that tombstone. */
	if (ck_hs_put(&hs, 0, &d) == false)
		ck_error("ck_hs_put after tombstone\n");

	if (ck_hs_get(&hs, 0, &d) != &d)
		ck_error("ck_hs_get after tombstone\n");

	if (hs.map->entries[1] != &d)
		ck_error("key not placed on the tombstone\n");

	ck_hs_deinit(&hs);
	return;
}

int
main(void)
{

	test(0, CK_HS_PROBE_INSERT);
	test(0, CK_HS_PROBE_TOMBSTONE);
	test(CK_HS_MODE_DELETE, CK_HS_PROBE_INSERT);
	test(CK_HS_MODE_DELETE, CK_HS_PROBE_TOMBSTONE);
	return 0;
}

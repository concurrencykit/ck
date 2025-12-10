/*
 * Copyright 2012 Samy Al Bahra.
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

#include <assert.h>
#include <ck_malloc.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../common.h"

/*
 * We tag keys and values so that we can check that callbacks receive
 * the correct kinds of arguments.
 */
#define KEY_TAG 0xAA000000
#define VAL_TAG 0xBB000000
#define TAGGED_KEY(key) ((key) | KEY_TAG)
#define TAGGED_VAL(val) ((val) | VAL_TAG)
#define GET_TAG(tagged) ((tagged) & 0xFF000000)

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
hs_hash(const void *key, unsigned long seed)
{
	const unsigned long *k = key;

	assert(GET_TAG(*k) == KEY_TAG);

	(void)seed;
	return *k;
}

static bool
hs_compare(const void *previous, const void *compare)
{
	const unsigned long *a = previous;
	const unsigned long *b = compare;

	assert(GET_TAG(*a) == KEY_TAG);
	assert(GET_TAG(*b) == KEY_TAG);

	return *a == *b;
}

static void *
replace_cl(void *vobj, void *cl)
{
	unsigned long *obj = vobj;
	unsigned long *repl = cl;

	assert(GET_TAG(*obj) == VAL_TAG);
	assert(GET_TAG(*repl) == VAL_TAG);

	return cl;
}

/*
 * Test object that contains tagged (val,key) pairs.  The tags are
 * used to check that the callbacks are called on the correct
 * things.
 */
struct test_obj {
	unsigned long val;
	unsigned long key;
};

#define TEST_OBJ_INITIALIZER(v, k) (struct test_obj) { .val = TAGGED_VAL(v), .key = TAGGED_KEY(k) }

static void
run_happy_path_test(void)
{
	ck_hs_t hs[1];
	struct ck_hs_init_options opts = CK_HS_INIT_OPTIONS_INITIALIZER;
	struct test_obj a, b, c;
	unsigned long key;
	void *ptr;

	opts.mode = CK_HS_MODE_SPMC | CK_HS_MODE_OBJECT;
	opts.hash_function = hs_hash;
	opts.compare = hs_compare;
	opts.allocator = &my_allocator;
	opts.seed = 1234;
	opts.key_offset = offsetof(struct test_obj, key);
	opts.capacity = 8;

	if (ck_hs_init_from_options(hs, &opts) == false)
		ck_error("ck_hs_init_from_options\n");

	ck_hs_gc(hs, 0, 0);

	/* Check that the hash is the identity. */
	a = TEST_OBJ_INITIALIZER(1000, 111);
	assert(hs_hash(&a.key, hs->seed) == TAGGED_KEY(111));

	ck_hs_gc(hs, 0, 0);

	/* Add `a` with key 111 */
	a = TEST_OBJ_INITIALIZER(65, 111);
	ptr = &ptr;
	assert(ck_hs_set(hs, hs_hash(&a.key, hs->seed), &a, &ptr) == true);
	assert(ptr == NULL);

	ck_hs_gc(hs, 0, 0);

	/* Add `b` with key 222 */
	b = TEST_OBJ_INITIALIZER(66, 222);
	ptr = &ptr;
	assert(ck_hs_set(hs, hs_hash(&b.key, hs->seed), &b, &ptr) == true);
	assert(ptr == NULL);

	ck_hs_gc(hs, 0, 0);

	/* Remove `a` */
	key = TAGGED_KEY(111);
	ptr = &ptr;
	ptr = ck_hs_remove(hs, hs_hash(&key, hs->seed), &key);
	assert(ptr == &a);

	ck_hs_gc(hs, 0, 0);

	/* Replace `b` with `c`. */
	c = TEST_OBJ_INITIALIZER(67, 222);
	ptr = &ptr;
	assert(ck_hs_set(hs, hs_hash(&c.key, hs->seed), &c, &ptr) == true);
	assert(ptr == &b);

	ck_hs_gc(hs, 0, 0);

	/*
	 * Check that `ck_hs_apply` takes key pointers and is called
	 * on object pointers.  The witness replaces `c` with `a`.
	 */
	key = TAGGED_KEY(222);
	a = TEST_OBJ_INITIALIZER(65, 222);
	assert(ck_hs_apply(hs, hs_hash(&key, hs->seed), &key, replace_cl, &a) == true);

	ck_hs_gc(hs, 0, 0);

	/*
	 * At this point `hs` only has `a` in it with key 222.
	 * Replace it with `b` using `ck_hs_fas`.
	 */
	b = TEST_OBJ_INITIALIZER(66, 222);
	ptr = &ptr;
	assert(ck_hs_fas(hs, hs_hash(&b.key, hs->seed), &b, &ptr));
	assert(ptr == &a);

	ck_hs_gc(hs, 0, 0);

	/*
	 * Check that `ck_hs_put` takes objects.
	 */
	c = TEST_OBJ_INITIALIZER(67, 333);
	assert(ck_hs_put(hs, hs_hash(&c.key, hs->seed), &c) == true);

	ck_hs_gc(hs, 0, 0);

	/* Also `ck_hs_put_unique`. */
	a = TEST_OBJ_INITIALIZER(65, 111);
	assert(ck_hs_put_unique(hs, hs_hash(&c.key, hs->seed), &a) == true);

	ck_hs_gc(hs, 0, 0);

	/* Check that `ck_hs_get` takes keys. */
	key = TAGGED_KEY(111);
	assert(ck_hs_get(hs, hs_hash(&key, hs->seed), &key) == &a);

	ck_hs_gc(hs, 0, 0);

	ck_hs_deinit(hs);

	return;
}

static void
run_invalid_opts_tests(void)
{
	ck_hs_t hs[1];
	struct ck_hs_init_options opts = CK_HS_INIT_OPTIONS_INITIALIZER;

	opts.mode = CK_HS_MODE_SPMC | CK_HS_MODE_OBJECT;
	opts.hash_function = hs_hash;
	opts.compare = hs_compare;
	opts.allocator = &my_allocator;
	opts.seed = 1234;
	opts.capacity = 16;

	/* Key offset can't be too large. */
	opts.key_offset = 65536;
	if (ck_hs_init_from_options(hs, &opts) == true)
		ck_error("ck_hs_init_from_options succeeded with too large"
		    " key offset");

	/* Mode must be OBJECT if key offset is not zero. */
	opts.mode = CK_HS_MODE_SPMC | CK_HS_MODE_DIRECT;
	opts.key_offset = 8;
	if (ck_hs_init_from_options(hs, &opts) == true)
		ck_error("ck_hs_init_from_options succeeded non-zero key offset"
		    " and DIRECT mode");

	return;
}

int
main(void)
{
	assert(sizeof(struct ck_hs_init_options) == CK_HS_INIT_OPTIONS_SIZE_V1);

	run_happy_path_test();
	run_invalid_opts_tests();

	return 0;
}


/*
 * Copyright 2012 Samy Al Bahra.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyrighs
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyrighs
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

const char *test[] = {"Samy", "Al", "Bahra", "dances", "in", "the", "wind.", "Once",
			"upon", "a", "time", "his", "gypsy", "ate", "one", "itsy",
			    "bitsy", "spider.", "What", "goes", "up", "must",
				"come", "down.", "What", "is", "down", "stays",
				    "down.", "A", "B", "C", "D", "E", "F", "G", "H",
					"I", "J", "K", "L", "M", "N", "O"};

const char *negative = "negative";

static unsigned long
hs_hash(const void *object, unsigned long seed)
{
	const char *c = object;
	unsigned long h;

	(void)seed;
	h = c[0];
	return h;
}

static bool
hs_compare(const void *previous, const void *compare)
{

	return strcmp(previous, compare) == 0;
}

int
main(void)
{
	const char *blob = "blobs";
	unsigned long h;
	ck_hs_t hs;
	size_t i;

	if (ck_hs_init(&hs, CK_HS_MODE_SPMC | CK_HS_MODE_OBJECT, hs_hash, hs_compare, &my_allocator, 8, 6602834) == false) {
		perror("ck_hs_init");
		exit(EXIT_FAILURE);
	}

	/* Test serial put semantics. */
	for (i = 0; i < sizeof(test) / sizeof(*test); i++) {
		h = test[i][0];
		ck_hs_put(&hs, h, test[i]);
		if (ck_hs_put(&hs, h, test[i]) == true) {
			ck_error("ERROR [1]: put must fail on collision.\n");
		}
	}

	/* Test grow semantics. */
	ck_hs_grow(&hs, 32);
	for (i = 0; i < sizeof(test) / sizeof(*test); i++) {
		h = test[i][0];
		if (ck_hs_put(&hs, h, test[i]) == true) {
			ck_error("ERROR [2]: put must fail on collision.\n");
		}

		if (ck_hs_get(&hs, h, test[i]) == NULL) {
			ck_error("ERROR: get must not fail\n");
		}
	}

	h = ULONG_MAX;
	if (ck_hs_put(&hs, h, blob) == false) {
		ck_error("ERROR: Duplicate put failed.\n");
	}

	if (ck_hs_put(&hs, h, blob) == true) {
		ck_error("ERROR: Duplicate put succeeded.\n");
	}

	/* Grow set and check get semantics. */
	ck_hs_grow(&hs, 128);
	for (i = 0; i < sizeof(test) / sizeof(*test); i++) {
		h = test[i][0];
		if (ck_hs_get(&hs, h, test[i]) == NULL) {
			ck_error("ERROR: get must not fail\n");
		}
	}

	/* Delete and check negative membership. */
	for (i = 0; i < sizeof(test) / sizeof(*test); i++) {
		void *r;

		h = test[i][0];
		if (ck_hs_get(&hs, h, test[i]) == NULL)
			continue;

		if (r = ck_hs_remove(&hs, h, test[i]), r == NULL) {
			ck_error("ERROR: remove must not fail\n");
		}

		if (strcmp(r, test[i]) != 0) {
			ck_error("ERROR: Removed incorrect node (%s != %s)\n", (char *)r, test[i]);
		}
	}

	/* Test replacement semantics. */
	for (i = 0; i < sizeof(test) / sizeof(*test); i++) {
		void *r;
		bool d;

		h = test[i][0];
		d = ck_hs_get(&hs, h, test[i]) != NULL;
		if (ck_hs_set(&hs, h, test[i], &r) == false) {
			ck_error("ERROR: Failed to set\n");
		}

		/* Expected replacement. */
		if (d == true && (r == NULL || strcmp(r, test[i]) != 0)) {
			ck_error("ERROR: Incorrect previous value: %s != %s\n",
			    test[i], (char *)r);
		}

		if (ck_hs_set(&hs, h, test[i], &r) == false) {
			ck_error("ERROR: Failed to set [1]\n");
		}

		if (strcmp(r, test[i]) != 0) {
			ck_error("ERROR: Invalid pointer: %s != %s\n", (char *)r, test[i]);
		}
	}

	return 0;
}


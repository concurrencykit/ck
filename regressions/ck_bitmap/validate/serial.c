/*
 * Copyright 2012-2014 Samy Al Bahra.
 * Copyright 2012-2014 AppNexus, Inc.
 * Copyright 2012 Shreyas Prasad.
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

#include <ck_bitmap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../common.h"

#ifndef STATIC_LENGTH
#define STATIC_LENGTH 256
#endif

static unsigned int length = 256;
static ck_bitmap_t *g_bits;

static void
check_iteration(ck_bitmap_t *bits, unsigned int len, bool initial)
{
	ck_bitmap_iterator_t iter;
	unsigned int i = 0, j;

	len += 1;
	if (initial == true) {
		if (bits == g_bits)
			len = length;
		else
			len = STATIC_LENGTH;
	}

	ck_bitmap_iterator_init(&iter, bits);
	for (j = 0; ck_bitmap_next(bits, &iter, &i) == true; j++) {
		if (i == j)
			continue;

		ck_error("[4] ERROR: Expected bit %u, got bit %u\n", j, i);
	}

	if (j != len) {
		ck_error("[5] ERROR: Expected length %u, got length %u\n", len, j);
	}

	return;
}

static void
test(ck_bitmap_t *bits, unsigned int n_length, bool initial)
{
	unsigned int i;

	for (i = 0; i < n_length; i++) {
		if (ck_bitmap_test(bits, i) == !initial) {
			ck_error("[0] ERROR [%u]: Expected %u got %u\n", i,
				initial, !initial);
		}
	}

	for (i = 0; i < n_length; i++) {
		ck_bitmap_set_mpmc(bits, i);
		if (ck_bitmap_test(bits, i) == false) {
			ck_error("[1] ERROR: Expected bit to be set: %u\n", i);
		}
		ck_bitmap_reset_mpmc(bits, i);
		if (ck_bitmap_test(bits, i) == true) {
			ck_error("[2] ERROR: Expected bit to be cleared: %u\n", i);
		}

		ck_bitmap_set_mpmc(bits, i);
		if (ck_bitmap_test(bits, i) == false) {
			ck_error("[3] ERROR: Expected bit to be set: %u\n", i);
		}

		check_iteration(bits, i, initial);
	}

	for (i = 0; i < n_length; i++) {
		if (ck_bitmap_test(bits, i) == false) {
			ck_error("[4] ERROR: Expected bit to be set: %u\n", i);
		}
	}

	ck_bitmap_clear(bits);

	for (i = 0; i < n_length; i++) {
		if (ck_bitmap_test(bits, i) == true) {
			ck_error("[4] ERROR: Expected bit to be reset: %u\n", i);
		}
	}

	return;
}

int
main(int argc, char *argv[])
{
	unsigned int bytes, base;

	if (argc >= 2) {
		length = atoi(argv[1]);
	}

	base = ck_bitmap_base(length);
	bytes = ck_bitmap_size(length);
	fprintf(stderr, "Configuration: %u bytes\n",
	    bytes);

	g_bits = malloc(bytes);
	memset(g_bits->map, 0xFF, base);
	ck_bitmap_init(g_bits, length, false);
	test(g_bits, length, false);

	memset(g_bits->map, 0x00, base);
	ck_bitmap_init(g_bits, length, true);
	test(g_bits, length, true);

	ck_bitmap_test(g_bits, length - 1);

	CK_BITMAP_INSTANCE(STATIC_LENGTH) sb;
	fprintf(stderr, "Static configuration: %zu bytes\n",
	    sizeof(sb));
	memset(CK_BITMAP_BUFFER(&sb), 0xFF, ck_bitmap_base(STATIC_LENGTH));
	CK_BITMAP_INIT(&sb, STATIC_LENGTH, false);
	test(CK_BITMAP(&sb), STATIC_LENGTH, false);
	memset(CK_BITMAP_BUFFER(&sb), 0x00, ck_bitmap_base(STATIC_LENGTH));
	CK_BITMAP_INIT(&sb, STATIC_LENGTH, true);
	test(CK_BITMAP(&sb), STATIC_LENGTH, true);

	CK_BITMAP_CLEAR(&sb);
	if (CK_BITMAP_TEST(&sb, 1) == true) {
		ck_error("ERROR: Expected bit to be reset.\n");
	}

	CK_BITMAP_SET_MPMC(&sb, 1);
	if (CK_BITMAP_TEST(&sb, 1) == false) {
		ck_error("ERROR: Expected bit to be set.\n");
	}

	CK_BITMAP_RESET_MPMC(&sb, 1);
	if (CK_BITMAP_TEST(&sb, 1) == true) {
		ck_error("ERROR: Expected bit to be reset.\n");
	}

	return 0;
}

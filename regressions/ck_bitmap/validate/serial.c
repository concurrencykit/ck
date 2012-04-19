/*
 * Copyright 2012 Samy Al Bahra.
 * Copyright 2012 AppNexus, Inc.
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

static unsigned int length = 256;

static void
test(ck_bitmap_t *bits, bool initial)
{
	unsigned int i;

	for (i = 0; i < length; i++) {
		if (ck_bitmap_test(bits, i) == !initial) {
			fprintf(stderr, "[4] ERROR: Expected bit to not be set: %u\n", i);
			exit(EXIT_FAILURE);
		}
	}

	for (i = 0; i < length; i++) {
		ck_bitmap_set_mpmc(bits, i);
		if (ck_bitmap_test(bits, i) == false) {
			fprintf(stderr, "[1] ERROR: Expected bit to be set: %u\n", i);
			exit(EXIT_FAILURE);
		}

		ck_bitmap_reset_mpmc(bits, i);
		if (ck_bitmap_test(bits, i) == true) {
			fprintf(stderr, "[2] ERROR: Expected bit to be cleared: %u\n", i);
			exit(EXIT_FAILURE);
		}

		ck_bitmap_set_mpmc(bits, i);
		if (ck_bitmap_test(bits, i) == false) {
			fprintf(stderr, "[3] ERROR: Expected bit to be set: %u\n", i);
			exit(EXIT_FAILURE);
		}
	}

	for (i = 0; i < length; i++) {
		if (ck_bitmap_test(bits, i) == false) {
			fprintf(stderr, "[4] ERROR: Expected bit to be set: %u\n", i);
			exit(EXIT_FAILURE);
		}
	}

	return;
}

int
main(int argc, char *argv[])
{
	unsigned int bytes;
	ck_bitmap_t bits;
	void *buffer;

	if (argc >= 2) {
		length = atoi(argv[1]);
	}

	bytes = ck_bitmap_size(length);
	fprintf(stderr, "Configuration: %u bytes\n\n",
	    bytes);

	buffer = malloc(bytes);
	memset(buffer, 0xFF, bytes);
	ck_bitmap_init(&bits, buffer, length, false);
	test(&bits, false);

	memset(buffer, 0xFF, bytes);
	ck_bitmap_init(&bits, buffer, length, true);
	test(&bits, true);

	ck_bitmap_test(&bits, length - 1);

	return 0;
}

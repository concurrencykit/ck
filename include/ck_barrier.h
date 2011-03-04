/*
 * Copyright 2011 Samy Al Bahra.
 * Copyright 2011 David Joseph.
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

#ifndef _CK_BARRIER_H
#define _CK_BARRIER_H

#include <ck_spinlock.h>

struct ck_barrier_centralized {
	unsigned int value;
	unsigned int sense;
};
typedef struct ck_barrier_centralized ck_barrier_centralized_t;

struct ck_barrier_centralized_state {
	unsigned int sense;
};
typedef struct ck_barrier_centralized_state ck_barrier_centralized_state_t;

#define CK_BARRIER_CENTRALIZED_INITIALIZER 	 {0, 0}
#define CK_BARRIER_CENTRALIZED_STATE_INITIALIZER {0}

void ck_barrier_centralized(ck_barrier_centralized_t *,
			    ck_barrier_centralized_state_t *,
			    unsigned int);

struct ck_barrier_combining_group {
	unsigned int k;
	unsigned int count;
	unsigned int sense;
	struct ck_barrier_combining_group *parent;
	struct ck_barrier_combining_group *lchild;
	struct ck_barrier_combining_group *rchild;
	struct ck_barrier_combining_group *next;
} CK_CC_CACHELINE;
typedef struct ck_barrier_combining_group ck_barrier_combining_group_t;

struct ck_barrier_combining_state {
	unsigned int sense;
};
typedef struct ck_barrier_combining_state ck_barrier_combining_state_t;

#define CK_BARRIER_COMBINING_STATE_INITIALIZER {~0}

struct ck_barrier_combining {
	struct ck_barrier_combining_group *root;
	ck_spinlock_fas_t mutex;
};
typedef struct ck_barrier_combining ck_barrier_combining_t;

void ck_barrier_combining_init(ck_barrier_combining_t *, ck_barrier_combining_group_t *);

void ck_barrier_combining_group_init(ck_barrier_combining_t *,
				     ck_barrier_combining_group_t *,
				     unsigned int);

void ck_barrier_combining(ck_barrier_combining_t *,
			  ck_barrier_combining_group_t *,
			  ck_barrier_combining_state_t *);

struct ck_barrier_dissemination_flags {
	unsigned int *tflags[2];
	unsigned int **pflags[2];
};
typedef struct ck_barrier_dissemination_flags ck_barrier_dissemination_flags_t;

struct ck_barrier_dissemination_state {
	int parity;
	unsigned int sense;
};
typedef struct ck_barrier_dissemination_state ck_barrier_dissemination_state_t;

#define CK_BARRIER_DISSEMINATION_STATE_INITIALIZER {0, ~0}

void ck_barrier_dissemination_flags_init(ck_barrier_dissemination_flags_t *,
				   	 int);

void ck_barrier_dissemination_state_init(ck_barrier_dissemination_state_t *);

int ck_barrier_dissemination_size(unsigned int);

void ck_barrier_dissemination(ck_barrier_dissemination_flags_t *,
			      ck_barrier_dissemination_state_t *,
			      int,
			      int);

#endif /* _CK_BARRIER_H */

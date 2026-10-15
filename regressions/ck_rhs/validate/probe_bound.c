/*
 * Copyright 2026 Michael Grunder.
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

#include <ck_malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Unlike serial.c, this test includes the implementation rather than linking
 * against it. Asserting that no descriptor is left marked in_rh requires the
 * private map layout, and that invariant has no black box equivalent: a leaked
 * in_rh only degrades Robin Hood balancing, so every key stays reachable.
 */
#include "../../../src/ck_rhs.c"

#include "../../common.h"

#define INITIAL_CAPACITY 32768
#define INITIAL_KEYS 23000
#define CHURN_OPERATIONS 7000
#define MAX_KEYS (INITIAL_KEYS + CHURN_OPERATIONS)
#define HASH_MASK UINT64_C(0x3fff)

struct key {
	unsigned long hash;
	unsigned long id;
};

static struct key keys[MAX_KEYS];
static unsigned int active[INITIAL_KEYS];
static bool fail_allocations;
static unsigned long failed_allocations;
static void *deferred_free;

static void *
test_malloc(size_t size)
{

	if (fail_allocations) {
		failed_allocations++;
		return NULL;
	}

	return malloc(size);
}

static void
test_free(void *pointer, size_t size, bool defer)
{

	(void)size;
	if (defer) {
		if (deferred_free != NULL)
			ck_error("ERROR: More than one deferred free is pending.\n");
		deferred_free = pointer;
		return;
	}

	free(pointer);
	return;
}

static struct ck_malloc allocator = {
	.malloc = test_malloc,
	.free = test_free
};

static unsigned long
key_hash(const void *object, unsigned long seed)
{
	const struct key *key = object;

	(void)seed;
	return key->hash;
}

static bool
key_compare(const void *left, const void *right)
{
	const struct key *a = left;
	const struct key *b = right;

	return a->id == b->id;
}

static uint64_t
random_next(uint64_t *state)
{
	uint64_t z;

	z = (*state += UINT64_C(0x9e3779b97f4a7c15));
	z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
	z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
	return z ^ (z >> 31);
}

/*
 * No descriptor may be left marked in_rh once an operation has completed. A
 * slot marked in_rh is skipped when choosing a Robin Hood relocation victim,
 * so a leaked mark permanently excludes that slot from rebalancing.
 */
static void
validate_no_in_rh(ck_rhs_t *rhs, unsigned long operation)
{
	struct ck_rhs_map *map = rhs->map;
	unsigned long i, marked = 0;

	for (i = 0; i <= map->mask; i++) {
		if (ck_rhs_in_rh(map, i) == true)
			marked++;
	}

	if (marked != 0) {
		ck_error("ERROR: %lu descriptor(s) left marked in_rh after "
		    "operation %lu.\n", marked, operation);
	}

	return;
}

enum grow_operation {
	GROW_FAS,
	GROW_SET,
	GROW_APPLY,
	GROW_OPERATIONS
};

struct apply_state {
	struct key *replacement;
	void *previous;
	unsigned int calls;
};

static void *
apply_replace(void *previous, void *closure)
{
	struct apply_state *state = closure;

	state->previous = previous;
	state->calls++;
	return state->replacement;
}

static void
validate_successful_grow(enum grow_operation operation)
{
	static const char *names[] = { "fas", "set", "apply" };
	struct key test[] = {
		{ 0, 0 },
		{ 1, 1 },
		{ 0, 2 },
		{ 0, 2 }
	};
	struct apply_state state = { &test[3], NULL, 0 };
	unsigned long capacity;
	ck_rhs_t rhs;
	void *previous = NULL;
	bool result = false;

	if (ck_rhs_init(&rhs, CK_RHS_MODE_SPMC | CK_RHS_MODE_OBJECT,
	    key_hash, key_compare, &allocator, 8, 0) == false) {
		ck_error("ERROR: Failed to initialize RHS for successful %s grow.\n",
		    names[operation]);
	}

	/*
	 * Put the target at probe distance three without Robin Hood balancing.
	 * Its normal probe selects test[1] as a relocation candidate; limiting
	 * that relocation to one probe forces ck_rhs_put_robin_hood to grow.
	 */
	if (ck_rhs_put(&rhs, test[0].hash, &test[0]) == false ||
	    ck_rhs_put(&rhs, test[1].hash, &test[1]) == false ||
	    ck_rhs_put_internal(&rhs, test[2].hash, &test[2],
	    CK_RHS_PROBE_NO_RH) == false) {
		ck_error("ERROR: Failed to prepare successful %s grow.\n",
		    names[operation]);
	}

	capacity = rhs.map->capacity;
	rhs.map->probe_limit = 1;
	switch (operation) {
	case GROW_FAS:
		result = ck_rhs_fas(&rhs, test[3].hash, &test[3], &previous);
		break;
	case GROW_SET:
		result = ck_rhs_set(&rhs, test[3].hash, &test[3], &previous);
		break;
	case GROW_APPLY:
		result = ck_rhs_apply(&rhs, test[3].hash, &test[3], apply_replace,
		    &state);
		previous = state.previous;
		break;
	default:
		ck_error("ERROR: Invalid successful grow operation.\n");
	}

	if (result == false || rhs.map->capacity <= capacity ||
	    previous != &test[2] ||
	    ck_rhs_get(&rhs, test[3].hash, &test[3]) != &test[3]) {
		ck_error("ERROR: Successful %s grow produced an invalid result.\n",
		    names[operation]);
	}
	if (operation == GROW_APPLY && state.calls != 1)
		ck_error("ERROR: Apply callback invoked %u times across growth.\n",
		    state.calls);
	validate_no_in_rh(&rhs, operation);
	ck_rhs_destroy(&rhs);
	free(deferred_free);
	deferred_free = NULL;
	return;
}

static void
validate_active(ck_rhs_t *rhs, unsigned long operation)
{
	unsigned int i;

	for (i = 0; i < INITIAL_KEYS; i++) {
		unsigned int key_index;

		key_index = active[i];
		if (ck_rhs_get(rhs, keys[key_index].hash, &keys[key_index]) !=
		    &keys[key_index]) {
			ck_error("ERROR: Active key %u is unreachable after operation "
			    "%lu.\n", key_index, operation);
		}
	}
}

int
main(void)
{
	struct ck_rhs_stat stat;
	uint64_t random = 1;
	ck_rhs_t rhs;
	struct key *found;
	unsigned int next_key;
	unsigned long i;
	unsigned int operation;

	for (operation = 0; operation < GROW_OPERATIONS; operation++)
		validate_successful_grow(operation);

	if (ck_rhs_init(&rhs, CK_RHS_MODE_SPMC | CK_RHS_MODE_OBJECT,
	    key_hash, NULL, &allocator, INITIAL_CAPACITY, 0) == false) {
		ck_error("ERROR: Failed to initialize RHS.\n");
	}

	fail_allocations = true;
	for (i = 0; i < INITIAL_KEYS; i++) {
		keys[i].hash = random_next(&random) & HASH_MASK;
		active[i] = i;
		if (ck_rhs_put(&rhs, keys[i].hash, &keys[i]) == false)
			ck_error("ERROR: Failed to insert initial key %lu.\n", i);
	}
	validate_active(&rhs, 0);
	validate_no_in_rh(&rhs, 0);

	for (i = 0, next_key = INITIAL_KEYS; i < CHURN_OPERATIONS; i++, next_key++) {
		unsigned int active_offset = random_next(&random) % INITIAL_KEYS;
		unsigned int old_key = active[active_offset];

		found = ck_rhs_get(&rhs, keys[old_key].hash, &keys[old_key]);
		if (found != &keys[old_key]) {
			ck_error("ERROR: Failed to find key %u before removal at "
			    "operation %lu.\n", old_key, i);
		}
		if (ck_rhs_remove(&rhs, found->hash, found) != found) {
			ck_error("ERROR: Failed to remove key %u at operation %lu.\n",
			    old_key, i);
		}

		keys[next_key].hash = random_next(&random) & HASH_MASK;
		if (ck_rhs_put(&rhs, keys[next_key].hash, &keys[next_key]) == false) {
			ck_error("ERROR: Failed to insert key %u at operation %lu.\n",
			    next_key, i);
		}
		active[active_offset] = next_key;
		validate_no_in_rh(&rhs, i);
	}

	validate_active(&rhs, CHURN_OPERATIONS);
	if (failed_allocations == 0)
		ck_error("ERROR: No allocation failure was exercised.\n");
	ck_rhs_stat(&rhs, &stat);
	printf("probe maximum: %u\n", stat.probe_maximum);

	ck_rhs_destroy(&rhs);
	return 0;
}

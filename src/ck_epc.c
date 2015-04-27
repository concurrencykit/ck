/*
 * Copyright 2015 John Esmet.
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

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <ck_epc.h>

void
ck_epc_init(ck_epc_t *epc)
{

	ck_epoch_init(&epc->epoch);

	return;
}

void
ck_epc_register(ck_epc_t *epc, ck_epc_record_t *record)
{

	memset(record, 0, sizeof(ck_epc_record_t));
	ck_epoch_register(&epc->epoch, &record->record);

	return;
}

void
ck_epc_unregister(ck_epc_t *epc, ck_epc_record_t *record)
{

	ck_epoch_unregister(&epc->epoch, &record->record);
	memset(record, 0, sizeof(ck_epc_record_t));

	return;
}

static inline bool
modular_less_than(unsigned int a, unsigned int b)
{

	return (int)(a - b) < 0;
}

static ck_epc_epoch_ref_t *
get_oldest_referenced_epoch_ref(ck_epc_record_t *record)
{
	ck_epc_epoch_ref_t *oldest;

	oldest = NULL;
	for (unsigned int i = 0; i < CK_EPC_GRACE; i++) {
		ck_epc_epoch_ref_t *ref;

		ref = &record->epoch_refs[i];
		if (ref->ref_count == 0) {
			continue;
		}

		if (oldest == NULL ||
		    modular_less_than(ref->epoch_value, oldest->epoch_value)) {
			oldest = ref;
		}
	}

	return oldest;
}

void
ck_epc_begin(ck_epc_t *epc, ck_epc_record_t *record, ck_epc_section_t *section)
{
	unsigned int epoch;
	unsigned int epoch_idx;
	ck_epc_epoch_ref_t *ref;

	/* Begin this record's epoch section if this is the first ref. */
	record->total_epoch_refs++;
	if (record->total_epoch_refs == 1) {
		ck_epoch_begin(&epc->epoch, &record->record);
	}

	/* Observe the current epoch value. */
	epoch = ck_pr_load_uint(&epc->epoch.epoch);

	epoch_idx = epoch % CK_EPC_GRACE;
	ref = &record->epoch_refs[epoch_idx];
	if (ref->ref_count == 0) {
		/*
		 * We are about to reference a new epoch.
		 *
		 * It is now safe to dispatch callbacks for on the deferral
		 * stack for `epoch % CK_EPOCH_LENGTH' (see implementation),
		 * since the last epoch to be allocated on that portion of the
		 * ring was at least 2 epochs older.
		 */
		ck_epoch_dispatch(&record->record, epoch);
		ref->epoch_value = epoch;
		ref->ref_count = 1;
	} else {
		assert(ref->epoch_value == epoch);
		ref->ref_count++;
	}

	section->epoch_idx = epoch_idx;

	return;
}

void
ck_epc_end(ck_epc_t *epc, ck_epc_record_t *record, ck_epc_section_t *section)
{
	ck_epc_epoch_ref_t *ref, *oldest_ref;

	assert(record->total_epoch_refs > 0);

	ref = &record->epoch_refs[section->epoch_idx];
	assert(ref->ref_count > 0);

	ref->ref_count--;
	if (ref->ref_count == 0) {
		unsigned int epoch;

		/*
		 * Find the next oldest epoch value with a non-zero ref count.
		 *
		 * If there are no more referenced epochs, then the oldest
		 * epoch is the current epoch, and we won't change the current
		 * epoch value at all.
		 */
		epoch = ck_pr_load_uint(&record->record.epoch);
		oldest_ref = get_oldest_referenced_epoch_ref(record);
		if (oldest_ref != NULL &&
		    modular_less_than(epoch, oldest_ref->epoch_value)) {
			/* Update this record's epoch value to match the oldest value. */
			ck_pr_fas_uint(&record->record.epoch, oldest_ref->epoch_value);
		}
	}

	/* End this record's epoch section if there are no more references. */
	record->total_epoch_refs--;
	if (record->total_epoch_refs == 0) {
		ck_epoch_end(&epc->epoch, &record->record);
	}

	return;
}

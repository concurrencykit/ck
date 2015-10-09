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

#ifndef CK_EPC_H
#define CK_EPC_H

/* 
 * Proxy collection specialized for epoch reclamation.
 *
 * It works by noting the current epoch value at the time of any read section
 * begin, incrementing the reference count for that epoch. When read sections
 * close, they decrement the reference count and possibly move their epoch
 * record's internal epoch value to the oldest still-referenced epoch value
 * in the record.
 */

#include <ck_epoch.h>

/*
 * We only need at most two reference counts, since any epc
 * record at epoch E will prevent the global epoch from advacing
 * passed E + 1. No new read sections will ever observe E + 2.
 */
#define CK_EPC_GRACE 2

typedef ck_epoch_cb_t ck_epc_cb_t;

typedef struct ck_epc_epoch_ref {
	unsigned int epoch_value;
	size_t ref_count;
} ck_epc_epoch_ref_t;

typedef struct ck_epc_entry {
	ck_epoch_entry_t entry;
} ck_epc_entry_t;

typedef struct ck_epc_record {
	ck_epoch_record_t record;

	/* Array of (epoch, refcount) pairs for this record. */
	struct ck_epc_epoch_ref epoch_refs[CK_EPC_GRACE];

	/* Number of total epc sections currently open on this record. */
	size_t total_epoch_refs;
} ck_epc_record_t;

typedef struct ck_epc_section {
	unsigned int epoch_idx;
} ck_epc_section_t;

typedef struct ck_epc {
	ck_epoch_t epoch;
} ck_epc_t;

void ck_epc_init(ck_epc_t *epc);

void ck_epc_register(ck_epc_t *epc, ck_epc_record_t *record);

void ck_epc_unregister(ck_epc_t *epc, ck_epc_record_t *record);

/*
 * Marks the beginning of an epc-protected section.
 */
void ck_epc_begin(ck_epc_t *epc, ck_epc_record_t *record, ck_epc_section_t *section);

/*
 * Marks the end of an epc-protected section
 */
void ck_epc_end(ck_epc_t *epc, ck_epc_record_t *record, ck_epc_section_t *section);

static inline void
ck_epc_call(ck_epc_t *epc, ck_epc_record_t *record, ck_epc_entry_t *entry,
    ck_epc_cb_t function)
{

	ck_epoch_call(&epc->epoch, &record->record, &entry->entry, function);

	return;
}

static inline void
ck_epc_barrier(ck_epc_t *epc, ck_epc_record_t *record)
{

	ck_epoch_barrier(&epc->epoch, &record->record);

	return;
}

#endif /* CK_EPC_H */

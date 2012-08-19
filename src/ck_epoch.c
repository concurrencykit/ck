/*
 * Copyright 2011-2012 Samy Al Bahra.
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

/*
 * The implementation here is inspired from the work described in:
 *   Fraser, K. 2004. Practical Lock-Freedom. PhD Thesis, University
 *   of Cambridge Computing Laboratory.
 */

#include <ck_backoff.h>
#include <ck_cc.h>
#include <ck_epoch.h>
#include <ck_pr.h>
#include <ck_stack.h>
#include <stdbool.h>

enum {
	CK_EPOCH_USED = 0,
	CK_EPOCH_FREE = 1
};

CK_STACK_CONTAINER(struct ck_epoch_record, record_next, ck_epoch_record_container)
CK_STACK_CONTAINER(struct ck_epoch_entry, stack_entry, ck_epoch_entry_container)

void
ck_epoch_init(struct ck_epoch *global, unsigned int threshold)
{

	ck_stack_init(&global->records);
	global->epoch = 1;
	global->n_free = 0;
	global->threshold = threshold;
	ck_pr_fence_store();
	return;
}

struct ck_epoch_record *
ck_epoch_recycle(struct ck_epoch *global)
{
	struct ck_epoch_record *record;
	ck_stack_entry_t *cursor;
	unsigned int status;

	if (ck_pr_load_uint(&global->n_free) == 0)
		return (NULL);

	CK_STACK_FOREACH(&global->records, cursor) {
		record = ck_epoch_record_container(cursor);

		if (ck_pr_load_uint(&record->status) == CK_EPOCH_FREE) {
			ck_pr_fence_load();
			status = ck_pr_fas_uint(&record->status, CK_EPOCH_USED);
			if (status == CK_EPOCH_FREE) {
				ck_pr_dec_uint(&global->n_free);
				return record;
			}
		}
	}

	return NULL;
}

void
ck_epoch_register(struct ck_epoch *global, struct ck_epoch_record *record)
{
	size_t i;

	record->status = CK_EPOCH_USED;
	record->active = 0;
	record->epoch = 0;
	record->delta = 0;
	record->n_pending = 0;
	record->n_peak = 0;
	record->n_reclamations = 0;
	record->global = global;

	for (i = 0; i < CK_EPOCH_LENGTH; i++)
		ck_stack_init(&record->pending[i]);

	ck_pr_fence_store();
	ck_stack_push_upmc(&global->records, &record->record_next);
	return;
}

void
ck_epoch_unregister(struct ck_epoch_record *record)
{
	size_t i;

	record->active = 0;
	record->epoch = 0;
	record->delta = 0;
	record->n_pending = 0;
	record->n_peak = 0;
	record->n_reclamations = 0;

	for (i = 0; i < CK_EPOCH_LENGTH; i++)
		ck_stack_init(&record->pending[i]);

	ck_pr_fence_store();
	ck_pr_store_uint(&record->status, CK_EPOCH_FREE);
	ck_pr_inc_uint(&record->global->n_free);
	return;
}

void
ck_epoch_tick(struct ck_epoch *global, struct ck_epoch_record *record)
{
	struct ck_epoch_record *c_record;
	ck_stack_entry_t *cursor;
	unsigned int g_epoch = ck_pr_load_uint(&global->epoch);

	CK_STACK_FOREACH(&global->records, cursor) {
		c_record = ck_epoch_record_container(cursor);
		if (ck_pr_load_uint(&c_record->status) == CK_EPOCH_FREE ||
		    c_record == record)
			continue;

		if (ck_pr_load_uint(&c_record->active) != 0 &&
		    ck_pr_load_uint(&c_record->epoch) != g_epoch)
			return;
	}

	/*
	 * If we have multiple writers, it is much easier to starve
	 * reclamation if we loop through the epoch domain. It may
	 * be worth it to add an SPMC variant to ck_epoch that relies
	 * on atomic increment operations instead.
	 */
	ck_pr_cas_uint(&global->epoch, g_epoch, (g_epoch + 1) & (CK_EPOCH_LENGTH - 1));
	return;
}

bool
ck_epoch_reclaim(struct ck_epoch_record *record)
{
	struct ck_epoch *global = record->global;
	unsigned int g_epoch = ck_pr_load_uint(&global->epoch);
	unsigned int epoch = record->epoch;
	ck_stack_entry_t *next, *cursor;

	if (epoch == g_epoch)
		return false;

	/*
	 * This means all threads with a potential reference to a
	 * hazard pointer will have a view as new as or newer than
	 * the calling thread. No active reference should exist to
	 * any object in the record's pending list.
	 */
	CK_STACK_FOREACH_SAFE(&record->pending[g_epoch], cursor, next) {
		struct ck_epoch_entry *entry = ck_epoch_entry_container(cursor);

		entry->destroy(entry);
		record->n_pending--;
		record->n_reclamations++;
	}

	ck_stack_init(&record->pending[g_epoch]);
	ck_pr_store_uint(&record->epoch, g_epoch);
	record->delta = 0;
	return true;
}

void
ck_epoch_write_begin(struct ck_epoch_record *record)
{
	struct ck_epoch *global = record->global;

	ck_pr_store_uint(&record->active, record->active + 1);

	/*
	 * In the case of recursive write sections, avoid ticking
	 * over global epoch.
	 */
	if (record->active > 1)
		return;

	ck_pr_fence_memory();
	for (;;) {
		/*
		 * Reclaim deferred objects if possible and
		 * acquire a new snapshot of the global epoch.
		 */
		if (ck_epoch_reclaim(record) == true)
			break;

		/*
		 * If we are above the global epoch record threshold,
		 * attempt to tick over the global epoch counter.
		 */
		if (++record->delta >= global->threshold) {
			record->delta = 0;
			ck_epoch_tick(global, record);
			continue;
		}

		break;
	}

	return;
}

void
ck_epoch_free(struct ck_epoch_record *record,
	      ck_epoch_entry_t *entry,
	      ck_epoch_destructor_t destroy)
{
	unsigned int epoch = ck_pr_load_uint(&record->epoch);
	struct ck_epoch *global = record->global;

	entry->destroy = destroy;
	ck_stack_push_spnc(&record->pending[epoch], &entry->stack_entry);
	record->n_pending += 1;

	if (record->n_pending > record->n_peak)
		record->n_peak = record->n_pending;

	if (record->n_pending >= global->threshold && ck_epoch_reclaim(record) == false)
		ck_epoch_tick(global, record);

	return;
}

void
ck_epoch_purge(struct ck_epoch_record *record)
{
	ck_backoff_t backoff = CK_BACKOFF_INITIALIZER;

	while (record->n_pending > 0) {
		ck_epoch_reclaim(record);
		ck_epoch_tick(record->global, record);
		if (record->n_pending > 0)
			ck_backoff_gb(&backoff);
	}

	return;
}


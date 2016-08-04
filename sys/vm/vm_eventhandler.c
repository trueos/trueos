
/*
 * Copyright (c) 2016 Matt Macy (mmacy@nextbsd.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/types.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/smp.h>

#include <machine/atomic.h>
#include <sys/queue.h>

#include <ck_epoch.h>
#include <vm/vm_eventhandler.h>

static MALLOC_DEFINE(M_VMEVENTHANDLER, "vme", "VM eventhandler");

static ck_epoch_t vme_epoch;

static void
vme_runtime_init(void *arg __unused)
{
	ck_epoch_record_t *record;
	int i;

	ck_epoch_init(&vme_epoch);

	/*
	 * Populate the epoch with 2*ncpus # of records
	 */
	for (i = 0; i < 2*mp_ncpus; i++) {
		record = malloc(sizeof *record, M_VMEVENTHANDLER, M_WAITOK);
		ck_epoch_register(&vme_epoch, record);
		ck_epoch_unregister(record);
	}	
}
SYSINIT(vm_eventhandler, SI_SUB_KTHREAD_PAGE, SI_ORDER_SECOND, vme_runtime_init, NULL);


static ck_epoch_record_t *
vme_get_record(void)
{
	ck_epoch_record_t *record;

	if (__predict_true((record = ck_epoch_recycle(&vme_epoch)) != NULL))
		return (record);

	
	/*
	 * In order to get to here every CPU has to have
	 * 2 outstanding operations in VM eventhandler
	 */
	record = malloc(sizeof *record, M_VMEVENTHANDLER, M_WAITOK);
	ck_epoch_register(&vme_epoch, record);
	return (record);
}

void
vm_eventhandler_register(vm_map_t map, struct vm_eventhandler *ve)
{
	struct vm_eventhandler_map *vem;

	vem = malloc(sizeof(*vem), M_VMEVENTHANDLER, M_WAITOK|M_ZERO);
	vm_map_lock(map);
	if (!vme_map_has_eh(map)) {
		mtx_init(&vem->vem_mtx, "vem lock", NULL, MTX_DEF);
		map->vem_map = vem;
		vem = NULL;
	}
	mtx_lock(&map->vem_map->vem_mtx);
	LIST_INSERT_HEAD(&map->vem_map->vem_head, ve, vme_entry);
	mtx_unlock(&map->vem_map->vem_mtx);
	vm_map_unlock(map);


	/* XXX How do we track the fact that we hold a reference to the map? */
	free(vem, M_VMEVENTHANDLER);
}

void
vm_eventhandler_deregister(vm_map_t map, struct vm_eventhandler *ve)
{
	ck_epoch_record_t *record;

	record = vme_get_record();
	if (!LIST_UNLINKED(ve, vme_entry)) {	
		ck_epoch_begin(record, NULL);
		if (ve->vme_ops.vme_exit)
			ve->vme_ops.vme_exit(ve, map);
		ck_epoch_end(record, NULL);

		mtx_lock(&map->vem_map->vem_mtx);
		LIST_REMOVE_EBR(ve, vme_entry);
		mtx_unlock(&map->vem_map->vem_mtx);
	}

	ck_epoch_barrier(record);
	ck_epoch_unregister(record);
}

int
vme_map_has_invalidate_page(vm_map_t map)
{
	ck_epoch_record_t *record;
	struct vm_eventhandler *vme;
	int found;
	
	found = 0;

	record = vme_get_record();
	ck_epoch_begin(record, NULL);
	LIST_FOREACH(vme, &map->vem_map->vem_head, vme_entry) {
		if (vme->vme_ops.vme_invalidate_page) {
			found = 1;
			break;
		}
	}
	ck_epoch_end(record, NULL);
	ck_epoch_unregister(record);
	return (found);
}

void
vme_exit_impl(vm_map_t map)
{
	ck_epoch_record_t *record;
	struct vm_eventhandler *vme;
	
	record = vme_get_record();
	ck_epoch_begin(record, NULL);
	LIST_FOREACH(vme, &map->vem_map->vem_head, vme_entry) {
		if (vme->vme_ops.vme_exit)
			vme->vme_ops.vme_exit(vme, map);
	}
	ck_epoch_end(record, NULL);

	mtx_lock(&map->vem_map->vem_mtx);
	while (__predict_false(!LIST_EMPTY(&map->vem_map->vem_head))) {
		vme = LIST_FIRST(&map->vem_map->vem_head);

		LIST_REMOVE_EBR(vme, vme_entry);
	}
	mtx_unlock(&map->vem_map->vem_mtx);
	ck_epoch_barrier(record);
	ck_epoch_unregister(record);
}

void
vme_invalidate_page_impl(vm_map_t map, vm_offset_t addr)
{
	ck_epoch_record_t *record;
	struct vm_eventhandler *vme;
	
	record = vme_get_record();
	ck_epoch_begin(record, NULL);
	LIST_FOREACH(vme, &map->vem_map->vem_head, vme_entry) {
		if (vme->vme_ops.vme_invalidate_page)
			vme->vme_ops.vme_invalidate_page(vme, map, addr);
	}
	ck_epoch_end(record, NULL);
	ck_epoch_unregister(record);
}

void
vme_invalidate_range_start_impl(vm_map_t map, vm_offset_t start, vm_offset_t end)
{
	ck_epoch_record_t *record;
	struct vm_eventhandler *vme;
	
	record = vme_get_record();
	ck_epoch_begin(record, NULL);
	LIST_FOREACH(vme, &map->vem_map->vem_head, vme_entry) {
		if (vme->vme_ops.vme_invalidate_page)
			vme->vme_ops.vme_invalidate_range_start(vme, map, start, end);
	}
	ck_epoch_end(record, NULL);
	ck_epoch_unregister(record);
}

void
vme_invalidate_range_end_impl(vm_map_t map, vm_offset_t start, vm_offset_t end)
{
	ck_epoch_record_t *record;
	struct vm_eventhandler *vme;
	
	record = vme_get_record();
	ck_epoch_begin(record, NULL);
	LIST_FOREACH(vme, &map->vem_map->vem_head, vme_entry) {
		if (vme->vme_ops.vme_invalidate_page)
			vme->vme_ops.vme_invalidate_range_end(vme, map, start, end);
	}
	ck_epoch_end(record, NULL);
	ck_epoch_unregister(record);
}

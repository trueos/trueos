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

#include <linux/types.h>
#include <linux/rcupdate.h>
#include <linux/srcu.h>



static ck_epoch_t lr_epoch;
static MALLOC_DEFINE(M_LRCU, "lrcu", "Linux RCU");

DPCPU_DEFINE(ck_epoch_record_t *, epoch_record);
CK_EPOCH_CONTAINER(struct rcu_head, epoch_entry, rcu_head_container)


static void
linux_rcu_runtime_init(void *arg __unused)
{
	ck_epoch_record_t *record;
	int i;

	ck_epoch_init(&lr_epoch);

	/*
	 * Populate the epoch with 2*ncpus # of records
	 */
	for (i = 0; i < 3*mp_ncpus; i++) {
		record = malloc(sizeof *record, M_LRCU, M_WAITOK|M_ZERO);
		ck_epoch_register(&lr_epoch, record);
		ck_epoch_unregister(record);
	}	
}
SYSINIT(linux_rcu, SI_SUB_KTHREAD_PAGE, SI_ORDER_SECOND, linux_rcu_runtime_init, NULL);

static ck_epoch_record_t *
rcu_get_record(int canblock)
{
	ck_epoch_record_t *record;

	if (__predict_true((record = ck_epoch_recycle(&lr_epoch)) != NULL))
		return (record);

	if (!canblock)
		return (NULL);

	record = malloc(sizeof *record, M_LRCU, M_WAITOK);
	ck_epoch_register(&lr_epoch, record);
	return (record);
}

static void
rcu_destroy_object(ck_epoch_entry_t *e)
{
	struct rcu_head *rcu = rcu_head_container(e);

	rcu->func(rcu);
}

static void
rcu_cleaner_func(void *context, int pending __unused)
{
	struct rcu_head *rcu = context;
	ck_epoch_record_t *record = rcu->epoch_record;

	rcu->epoch_record = NULL;
	ck_epoch_barrier(record);
}

void
__rcu_read_lock(void)
{
	ck_epoch_record_t *record;

	critical_enter();
	record = ck_epoch_recycle(&lr_epoch);
	DPCPU_SET(epoch_record, record);
	MPASS(record != NULL);

	ck_epoch_begin(record, NULL);
}

void
__rcu_read_unlock(void)
{
	ck_epoch_record_t *record;

	record = DPCPU_GET(epoch_record);
	ck_epoch_end(record, NULL);
	ck_epoch_unregister(record);
	critical_exit();
}

void
rcu_barrier(void)
{
	ck_epoch_record_t *record;

	record = rcu_get_record(1);
	ck_epoch_barrier(record);
}

void
call_rcu(struct rcu_head *ptr, rcu_callback_t func)
{
	ck_epoch_record_t *record;

	critical_enter();
	record = ck_epoch_recycle(&lr_epoch);
	MPASS(record != NULL);
	ptr->func = func;
	ptr->epoch_record = record;
	ck_epoch_call(record, &ptr->epoch_entry, rcu_destroy_object);
	TASK_INIT(&ptr->task, 0, rcu_cleaner_func, ptr);
	taskqueue_enqueue(taskqueue_fast, &ptr->task);

	critical_exit();
}

int
init_srcu_struct(struct srcu_struct *srcu)
{
	ck_epoch_record_t *record;
	
	record = rcu_get_record(1);
	srcu->ss_epoch_record = record;
	return (0);
}

void
cleanup_srcu_struct(struct srcu_struct *srcu)
{
	ck_epoch_record_t *record;
	
	record = srcu->ss_epoch_record;
	srcu->ss_epoch_record = NULL;
	ck_epoch_unregister(record);
}

int
srcu_read_lock(struct srcu_struct *srcu)
{
	ck_epoch_begin(srcu->ss_epoch_record, NULL);
	return (0);
}

void
srcu_read_unlock(struct srcu_struct *srcu, int key __unused)
{
	ck_epoch_end(srcu->ss_epoch_record, NULL);
}

void
synchronize_srcu(struct srcu_struct *srcu)
{
	ck_epoch_synchronize(srcu->ss_epoch_record);
}

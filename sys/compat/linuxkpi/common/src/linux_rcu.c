/*-
 * Copyright (c) 2017 Hans Petter Selasky
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>

#include <linux/rcupdate.h>
#include <linux/srcu.h>
#include <linux/slab.h>
#include <linux/kernel.h>

struct callback_head;

struct linux_rcu_head {
	struct mtx lock;
	struct task task;
	STAILQ_HEAD(, callback_head) head;
} __aligned(CACHE_LINE_SIZE);

struct linux_rcu_cpu_record {
	struct mtx sync_lock;
} __aligned(CACHE_LINE_SIZE);

struct callback_head {
	STAILQ_ENTRY(callback_head) entry;
	rcu_callback_t func;
};

/*
 * Verify that "struct rcu_head" is big enough to hold "struct
 * callback_head". This avoids header file pollution in the
 * LinuxKPI.
 */
CTASSERT(sizeof(struct rcu_head) == sizeof(struct callback_head));

static DPCPU_DEFINE(struct linux_rcu_cpu_record, linux_rcu_cpu_record);
static struct linux_rcu_head linux_rcu_head;

static void linux_rcu_cleaner_func(void *, int);

static void
linux_rcu_runtime_init(void *arg __unused)
{
	struct linux_rcu_head *head = &linux_rcu_head;
	int i;

	mtx_init(&head->lock, "LRCU-HEAD", NULL, MTX_DEF);
	TASK_INIT(&head->task, 0, linux_rcu_cleaner_func, NULL);
	STAILQ_INIT(&head->head);

	/* setup writer records */
	CPU_FOREACH(i) {
		struct linux_rcu_cpu_record *record;

		record = &DPCPU_ID_GET(i, linux_rcu_cpu_record);

		mtx_init(&record->sync_lock, "LRCU-NS-SYNC", NULL, MTX_DEF | MTX_RECURSE);
	}
}
SYSINIT(linux_rcu_runtime, SI_SUB_LOCK, SI_ORDER_SECOND, linux_rcu_runtime_init, NULL);

static void
linux_rcu_runtime_uninit(void *arg __unused)
{
	struct linux_rcu_head *head = &linux_rcu_head;
	int i;

	/* make sure all callbacks have been called */
	linux_rcu_barrier();

	/* destroy all writer record mutexes */
	CPU_FOREACH(i) {
		struct linux_rcu_cpu_record *record;

		record = &DPCPU_ID_GET(i, linux_rcu_cpu_record);

		mtx_destroy(&record->sync_lock);
	}
	mtx_destroy(&head->lock);
}
SYSUNINIT(linux_rcu_runtime, SI_SUB_LOCK, SI_ORDER_SECOND, linux_rcu_runtime_uninit, NULL);

static inline void
linux_rcu_synchronize_sub(struct linux_rcu_cpu_record *record)
{
	mtx_lock(&record->sync_lock);
	mtx_unlock(&record->sync_lock);
}

static void
linux_rcu_cleaner_func(void *context, int pending __unused)
{
	struct linux_rcu_head *head = &linux_rcu_head;
	struct callback_head *rcu;
	STAILQ_HEAD(, callback_head) temp_head;

	/* move current callbacks into own queue */
	mtx_lock(&head->lock);
	STAILQ_INIT(&temp_head);
	STAILQ_CONCAT(&temp_head, &head->head);
	mtx_unlock(&head->lock);

	/* synchronize */
	linux_synchronize_rcu();

	/* dispatch all callbacks, if any */
	while ((rcu = STAILQ_FIRST(&temp_head)) != NULL) {
		uintptr_t offset;

		STAILQ_REMOVE_HEAD(&temp_head, entry);

		offset = (uintptr_t)rcu->func;

		if (offset < LINUX_KFREE_RCU_OFFSET_MAX)
			kfree((char *)rcu - offset);
		else
			rcu->func((struct rcu_head *)rcu);
	}
}

void
linux_rcu_read_lock(void)
{
	struct linux_rcu_cpu_record *record;

	/*
	 * Pin thread to current CPU so that the unlock code gets the
	 * same per-CPU reader epoch record:
	 */
	sched_pin();

	record = &DPCPU_GET(linux_rcu_cpu_record);

	mtx_lock(&record->sync_lock);
}

void
linux_rcu_read_unlock(void)
{
	struct linux_rcu_cpu_record *record;

	record = &DPCPU_GET(linux_rcu_cpu_record);

	mtx_unlock(&record->sync_lock);

	sched_unpin();
}

void
linux_synchronize_rcu(void)
{
	int i;

	CPU_FOREACH(i) {
		struct linux_rcu_cpu_record *record;

		record = &DPCPU_ID_GET(i, linux_rcu_cpu_record);

		linux_rcu_synchronize_sub(record);
	}
}

void
linux_rcu_barrier(void)
{
	struct linux_rcu_head *head = &linux_rcu_head;

	linux_synchronize_rcu();

	/* wait for callbacks to complete */
	taskqueue_drain(taskqueue_fast, &head->task);
}

void
linux_call_rcu(struct rcu_head *context, rcu_callback_t func)
{
	struct callback_head *rcu = (struct callback_head *)context;
	struct linux_rcu_head *head = &linux_rcu_head;

	mtx_lock(&head->lock);
	rcu->func = func;
	STAILQ_INSERT_TAIL(&head->head, rcu, entry);
	taskqueue_enqueue(taskqueue_fast, &head->task);
	mtx_unlock(&head->lock);
}

int
init_srcu_struct(struct srcu_struct *srcu)
{
	sx_init(&srcu->sx, "SleepableRCU");
	return (0);
}

void
cleanup_srcu_struct(struct srcu_struct *srcu)
{
	sx_destroy(&srcu->sx);
}

int
srcu_read_lock(struct srcu_struct *srcu)
{
	sx_slock(&srcu->sx);
	return (0);
}

void
srcu_read_unlock(struct srcu_struct *srcu, int key __unused)
{
	sx_sunlock(&srcu->sx);
}

void
synchronize_srcu(struct srcu_struct *srcu)
{
	sx_xlock(&srcu->sx);
	sx_xunlock(&srcu->sx);
}

void
srcu_barrier(struct srcu_struct *srcu)
{
	sx_xlock(&srcu->sx);
	sx_xunlock(&srcu->sx);
}

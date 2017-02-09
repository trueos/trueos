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

#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/gtaskqueue.h>
#include <sys/proc.h>
#include <sys/sched.h>

#include <linux/interrupt.h>

#define	TASKLET_IDLE 0
#define	TASKLET_BUSY 1
#define	TASKLET_EXEC 2
#define	TASKLET_LOOP 3

#define	TASKLET_ST_CMPSET(ts, old, new)	\
	atomic_cmpset_ptr((volatile uintptr_t *)&(ts)->entry.tqe_prev, old, new)

#define	TASKLET_ST_SET(ts, new)	\
	atomic_store_rel_ptr((volatile uintptr_t *)&(ts)->entry.tqe_prev, new)

struct tasklet_worker {
	spinlock_t lock;
	TAILQ_HEAD(, tasklet_struct) head;
	struct grouptask gtask;
} __aligned(CACHE_LINE_SIZE);

static DPCPU_DEFINE(struct tasklet_worker, tasklet_worker);

static void
tasklet_handler(void *arg)
{
	struct tasklet_worker *tw;
	struct tasklet_struct *ts;

	MPASS(curcpu == (intptr_t)arg);

	tw = &DPCPU_GET(tasklet_worker);

	while (1) {
		spin_lock(&tw->lock);
		ts = TAILQ_FIRST(&tw->head);
		if (ts != NULL) {
			TAILQ_REMOVE(&tw->head, ts, entry);
			spin_unlock(&tw->lock);
		} else {
			spin_unlock(&tw->lock);
			break;
		}

		do {
			/* reset executing state */
			TASKLET_ST_SET(ts, TASKLET_EXEC);

			ts->func(ts->data);

		} while (TASKLET_ST_CMPSET(ts, TASKLET_EXEC, TASKLET_IDLE) == 0);
	}
}

static void
tasklet_subsystem_init(void *arg __unused)
{
	struct tasklet_worker *tw;
	char buf[32];
	int i;

	CPU_FOREACH(i) {
		if (CPU_ABSENT(i))
			continue;

		tw = DPCPU_ID_PTR(i, tasklet_worker);

		spin_lock_init(&tw->lock);
		TAILQ_INIT(&tw->head);

		GROUPTASK_INIT(&tw->gtask, 0, tasklet_handler,
		    (void *)(uintptr_t)i);

		snprintf(buf, sizeof(buf), "softirq%d", i);
		taskqgroup_attach_cpu(qgroup_softirq, &tw->gtask,
		    "tasklet", i, -1, buf);
	}
}
SYSINIT(linux_tasklet, SI_SUB_KTHREAD_PAGE, SI_ORDER_SECOND, tasklet_subsystem_init, NULL);

void
tasklet_init(struct tasklet_struct *ts,
    tasklet_func_t *func, unsigned long data)
{
	ts->entry.tqe_prev = NULL;
	ts->entry.tqe_next = NULL;
	ts->func = func;
	ts->data = data;
}

void
local_bh_enable(void)
{
	sched_unpin();
}

void
local_bh_disable(void)
{
	sched_pin();
}

void
linux_tasklet_schedule(struct tasklet_struct *ts)
{
	struct tasklet_worker *tw;

	if (TASKLET_ST_CMPSET(ts, TASKLET_EXEC, TASKLET_LOOP)) {
		/* tasklet_handler() will loop */
	} else if (TASKLET_ST_CMPSET(ts, TASKLET_IDLE, TASKLET_BUSY)) {
		tw = &DPCPU_GET(tasklet_worker);

		spin_lock(&tw->lock);
		/* enqueue tasklet */
		TAILQ_INSERT_TAIL(&tw->head, ts, entry);
		/* schedule worker */
		GROUPTASK_ENQUEUE(&tw->gtask);
		spin_unlock(&tw->lock);
	} else {
		/*
		 * The three cases that end up here:
		 * ===============================
		 *
		 * 1) The tasklet is now executing or did execute fully.
		 * 2) The tasklet is already queued.
		 * 3) The tasklet is already set to loop.
		 */
	}
}

void
tasklet_kill(struct tasklet_struct *ts)
{

	/* wait until tasklet is no longer busy */
	while (TASKLET_ST_CMPSET(ts, TASKLET_IDLE, TASKLET_IDLE) == 0)
		pause("W", 1);
}

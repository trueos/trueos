/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2015 Mellanox Technologies, Ltd.
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
 *
 * $FreeBSD$
 */
#ifndef	_LINUX_WORKQUEUE_H_
#define	_LINUX_WORKQUEUE_H_

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/threads.h>
#include <linux/atomic.h>

#include <sys/taskqueue.h>

struct work_struct;
typedef void (*work_func_t)(struct work_struct *work);

enum {
	WORK_CPU_UNBOUND = NR_CPUS,
};

enum {
	WQ_UNBOUND		= 1 << 1, /* not bound to any cpu */
	WQ_HIGHPRI		= 1 << 4, /* high priority */
	WQ_MAX_ACTIVE		= 512,	  /* I like 512, better ideas? */
	WQ_MAX_UNBOUND_PER_CPU	= 4,	  /* 4 * #cpus for unbound wq */
};

#define	WQ_UNBOUND_MAX_ACTIVE \
	max_t(int, WQ_MAX_ACTIVE, mp_ncpus * WQ_MAX_UNBOUND_PER_CPU)

struct work_exec {
	TAILQ_ENTRY(work_exec) entry;
	struct work_struct *target;
};

#define	WQ_EXEC_LOCK(wq) mtx_lock(&(wq)->exec_mtx)
#define	WQ_EXEC_UNLOCK(wq) mtx_unlock(&(wq)->exec_mtx)

struct workqueue_struct {
	struct taskqueue	*taskqueue;
	struct mtx		exec_mtx;
	TAILQ_HEAD(, work_exec)	exec_head;
	atomic_t		draining;
};

struct work_struct {
	struct task 		work_task;
	struct workqueue_struct	*work_queue;
	work_func_t		func;
	atomic_t		state;
};

#define	DECLARE_WORK(name, fn)				\
	struct work_struct name = { .func = (fn) }

struct delayed_work {
	struct work_struct	work;
	struct timer_list	timer;
	int			cpu;
};

extern struct workqueue_struct *system_wq;
extern struct workqueue_struct *system_long_wq;
extern struct workqueue_struct *system_unbound_wq;
extern struct workqueue_struct *system_power_efficient_wq;

static inline void destroy_work_on_stack(struct work_struct *work) { }
static inline void destroy_delayed_work_on_stack(struct delayed_work *work) { }

extern void linux_work_fn(void *, int);
extern void linux_flush_fn(void *, int);
extern void linux_delayed_work_timer_fn(unsigned long __data);
extern struct workqueue_struct *linux_create_workqueue_common(const char *, int);
extern void destroy_workqueue(struct workqueue_struct *);
extern bool queue_work_on(int cpu, struct workqueue_struct *wq, struct work_struct *work);
extern bool queue_delayed_work_on(int cpu, struct workqueue_struct *wq,
				  struct delayed_work *dwork, unsigned long delay);
extern bool cancel_delayed_work(struct delayed_work *dwork);
extern bool cancel_work_sync(struct work_struct *work);
extern bool cancel_delayed_work_sync(struct delayed_work *dwork);

static inline struct delayed_work *
to_delayed_work(struct work_struct *work)
{
 	return container_of(work, struct delayed_work, work);
}

#define	INIT_WORK(work, fn) 	 					\
do {									\
	(work)->func = (fn);						\
	(work)->work_queue = NULL;					\
	atomic_set(&(work)->state, 0);					\
	TASK_INIT(&(work)->work_task, 0, linux_work_fn, (work));	\
} while (0)

#define INIT_WORK_ONSTACK(...) INIT_WORK(__VA_ARGS__)

#define	INIT_DELAYED_WORK(_work, func)					\
do {									\
	INIT_WORK(&(_work)->work, func);				\
	setup_timer(&(_work)->timer, linux_delayed_work_timer_fn,	\
		    (unsigned long)(_work));				\
} while (0)

#define	INIT_DEFERRABLE_WORK(...) INIT_DELAYED_WORK(__VA_ARGS__)

#define	flush_scheduled_work(void) flush_taskqueue(taskqueue_thread)

static inline int
queue_work(struct workqueue_struct *wq, struct work_struct *work)
{

	return (queue_work_on(WORK_CPU_UNBOUND, wq, work));
}

static inline int
schedule_work(struct work_struct *work)
{

	return (queue_work_on(WORK_CPU_UNBOUND, system_wq, work));
}

static inline int
queue_delayed_work(struct workqueue_struct *wq, struct delayed_work *dwork,
    unsigned long delay)
{

	return (queue_delayed_work_on(WORK_CPU_UNBOUND, wq, dwork, delay));
}

static inline bool
schedule_delayed_work_on(int cpu, struct delayed_work *dwork,
					    unsigned long delay)
{

	return (queue_delayed_work_on(cpu, system_wq, dwork, delay));
}

static inline bool
schedule_delayed_work(struct delayed_work *dwork,
    unsigned long delay)
{

	return (queue_delayed_work(system_wq, dwork, delay));
}

#define	create_singlethread_workqueue(name)				\
	linux_create_workqueue_common(name, 1)

#define	create_workqueue(name)						\
	linux_create_workqueue_common(name, mp_ncpus)

#define	alloc_ordered_workqueue(name, flags)				\
	linux_create_workqueue_common(name, 1)

#define	alloc_workqueue(name, flags, max_active)			\
	linux_create_workqueue_common(name, max_active)

#define	flush_workqueue(wq)	flush_taskqueue((wq)->taskqueue)

static inline void
flush_taskqueue(struct taskqueue *tq)
{
	struct task flushtask;

	PHOLD(curproc);
	TASK_INIT(&flushtask, 0, linux_flush_fn, NULL);
	taskqueue_enqueue(tq, &flushtask);
	taskqueue_drain(tq, &flushtask);
	PRELE(curproc);
}

static inline void
drain_workqueue(struct workqueue_struct *wq)
{
	atomic_inc(&wq->draining);
	flush_taskqueue(wq->taskqueue);
	atomic_dec(&wq->draining);
}

static inline bool
mod_delayed_work(struct workqueue_struct *wq, struct delayed_work *dwork,
    unsigned long delay)
{
	bool retval;

	retval = cancel_delayed_work(dwork);
	queue_delayed_work(wq, dwork, delay);
	return (retval);
}

extern bool flush_work(struct work_struct *);
extern bool flush_delayed_work(struct delayed_work *);
extern bool work_pending(struct work_struct *);
extern bool work_busy(struct work_struct *);

static inline bool
delayed_work_pending(struct delayed_work *dwork)
{

	return (work_pending(&dwork->work));
}

#endif	/* _LINUX_WORKQUEUE_H_ */

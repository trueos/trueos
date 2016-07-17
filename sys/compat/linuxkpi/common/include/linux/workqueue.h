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

#define work_data_bits(work) ((unsigned long *)(&(work)->data))

enum {
	WORK_STRUCT_PENDING_BIT	= 0,	/* work item is pending execution */	

	WORK_STRUCT_PWQ_BIT	= 2,	/* data points to pwq */
	WORK_STRUCT_PWQ		= 1 << WORK_STRUCT_PWQ_BIT,
	WORK_STRUCT_PENDING	= 1 << WORK_STRUCT_PENDING_BIT,

	WORK_STRUCT_COLOR_BITS	= 4,
	WORK_STRUCT_COLOR_SHIFT	= 4,	/* color for workqueue flushing */
	WORK_STRUCT_FLAG_BITS	= WORK_STRUCT_COLOR_SHIFT +
				  WORK_STRUCT_COLOR_BITS,

	WORK_OFFQ_FLAG_BASE	= WORK_STRUCT_COLOR_SHIFT,
	WORK_OFFQ_FLAG_BITS	= 1,
	WORK_OFFQ_POOL_SHIFT	= WORK_OFFQ_FLAG_BASE + WORK_OFFQ_FLAG_BITS,
	WORK_OFFQ_LEFT		= BITS_PER_LONG - WORK_OFFQ_POOL_SHIFT,
	WORK_OFFQ_POOL_BITS	= WORK_OFFQ_LEFT <= 31 ? WORK_OFFQ_LEFT : 31,
	WORK_OFFQ_POOL_NONE	= (1LU << WORK_OFFQ_POOL_BITS) - 1,

	WORK_STRUCT_FLAG_MASK	= (1UL << WORK_STRUCT_FLAG_BITS) - 1,
	WORK_STRUCT_WQ_DATA_MASK = ~WORK_STRUCT_FLAG_MASK,
	WORK_STRUCT_NO_POOL	= (unsigned long)WORK_OFFQ_POOL_NONE << WORK_OFFQ_POOL_SHIFT,


	__WORK_OFFQ_CANCELING	= WORK_OFFQ_FLAG_BASE,
	WORK_OFFQ_CANCELING	= (1 << __WORK_OFFQ_CANCELING),
	
	WORK_BUSY_PENDING	= 1 << 0,
	WORK_BUSY_RUNNING	= 1 << 1,
	WORK_CPU_UNBOUND	= NR_CPUS,


};

enum {
	WQ_UNBOUND		= 1 << 1, /* not bound to any cpu */
	WQ_HIGHPRI		= 1 << 4, /* high priority */
	WQ_MAX_ACTIVE		= 512,	  /* I like 512, better ideas? */
	WQ_MAX_UNBOUND_PER_CPU	= 4,	  /* 4 * #cpus for unbound wq */
};

#define WQ_UNBOUND_MAX_ACTIVE	\
	max_t(int, WQ_MAX_ACTIVE, mp_ncpus * WQ_MAX_UNBOUND_PER_CPU)

struct workqueue_struct {
	struct taskqueue	*taskqueue;
	atomic_t		draining;
};

struct work_struct {
	atomic_long_t data;
	struct	task 		work_task;
	struct taskqueue	*taskqueue;
	void			(*fn)(struct work_struct *);
};

#define WORK_DATA_INIT()	ATOMIC_LONG_INIT(WORK_STRUCT_NO_POOL)
#define WORK_DATA_STATIC_INIT()	\
	ATOMIC_LONG_INIT(WORK_STRUCT_NO_POOL | WORK_STRUCT_STATIC)


#define LINUX_WORK_INITIALIZER(n, f) {					\
	.data = WORK_DATA_STATIC_INIT(),				\
	}

#define DECLARE_WORK(n, f)						\
	struct work_struct n = LINUX_WORK_INITIALIZER(n, f);

typedef __typeof(((struct work_struct *)0)->fn) work_func_t;

struct delayed_work {
	struct work_struct	work;
	struct timer_list	timer;
	struct workqueue_struct *wq;
	int			cpu;
};

extern struct workqueue_struct *system_wq;
extern struct workqueue_struct *system_highpri_wq;
extern struct workqueue_struct *system_long_wq;
extern struct workqueue_struct *system_unbound_wq;
extern struct workqueue_struct *system_freezable_wq;
extern struct workqueue_struct *system_power_efficient_wq;
extern struct workqueue_struct *system_freezable_power_efficient_wq;


extern void linux_work_fn(void *, int);
extern void linux_flush_fn(void *, int);
extern void linux_delayed_work_timer_fn(unsigned long __data);
extern struct workqueue_struct *linux_create_workqueue_common(const char *, int);
extern void destroy_workqueue(struct workqueue_struct *);
extern void linux_queue_work(int cpu, struct workqueue_struct *wq, struct work_struct *work);
extern bool queue_work_on(int cpu, struct workqueue_struct *wq, struct work_struct *work);
extern bool queue_delayed_work_on(int cpu, struct workqueue_struct *wq,
				  struct delayed_work *dwork, unsigned long delay);
extern bool cancel_delayed_work(struct delayed_work *dwork);
extern int cancel_work_sync(struct work_struct *work);
extern int cancel_delayed_work_sync(struct delayed_work *dwork);

static inline struct delayed_work *
to_delayed_work(struct work_struct *work)
{
 	return container_of(work, struct delayed_work, work);
}

#define	INIT_WORK(work, func) 	 					\
do {									\
	(work)->fn = (func);						\
	(work)->taskqueue = NULL;					\
	(work)->data = (atomic_long_t) WORK_DATA_INIT();		\
	TASK_INIT(&(work)->work_task, 0, linux_work_fn, (work));	\
} while (0)

#define	INIT_DELAYED_WORK(_work, func)					\
do {									\
	INIT_WORK(&(_work)->work, func);				\
	setup_timer(&(_work)->timer, linux_delayed_work_timer_fn,	\
		    (unsigned long)(_work));				\
} while (0)

#define	INIT_DEFERRABLE_WORK(...) INIT_DELAYED_WORK(__VA_ARGS__)

#define	flush_scheduled_work()	flush_taskqueue(taskqueue_thread)

#define work_pending(work) \
	test_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work))

#define delayed_work_pending(w) \
	work_pending(&(w)->work)

static inline int
queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
	return (queue_work_on(WORK_CPU_UNBOUND, wq, work));
}

static inline int
schedule_work(struct work_struct *work)
{
	work->taskqueue = system_wq->taskqueue;
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
	cancel_delayed_work(dwork);
	queue_delayed_work(wq, dwork, delay);
	return false;
}


static inline bool
flush_work(struct work_struct *work)
{
	struct taskqueue *tq;
	
	if ((tq = work->taskqueue) != NULL) {
		flush_taskqueue(tq);
		return (true);
	}
	return (false);
}

static inline bool
flush_delayed_work(struct delayed_work *dwork)
{

	if (del_timer_sync(&dwork->timer))
		linux_queue_work(dwork->cpu, dwork->wq, &dwork->work);
	return (flush_work(&dwork->work));
}

static inline unsigned int
work_busy(struct work_struct *work)
{
	struct task *ta;
	struct taskqueue *tq;
	int rc;

	DODGY();
	rc = 0;
	ta = &work->work_task;
	tq = work->taskqueue;

	if (work_pending(work))
		rc |= WORK_BUSY_PENDING;
	/*
	 * There's currently no interface to query if a task running
	 */
#ifdef notyet
	if (tq != NULL) {

	}
#endif	
	return (rc);
}
#endif	/* _LINUX_WORKQUEUE_H_ */

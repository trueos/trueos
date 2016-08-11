/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2016 Mellanox Technologies, Ltd.
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
#ifndef	_LINUX_KTHREAD_H_
#define	_LINUX_KTHREAD_H_

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/sleepqueue.h>

#include <linux/slab.h>

#include <linux/mm_types.h>


#define	MAX_SCHEDULE_TIMEOUT	LONG_MAX

#define	TASK_RUNNING		0
#define	TASK_INTERRUPTIBLE	1
#define	TASK_UNINTERRUPTIBLE	2
#define	TASK_KILLABLE           3
#define	TASK_DEAD		64
#define	TASK_WAKEKILL		128
#define	TASK_WAKING		256

#define	TASK_SHOULD_STOP	1
#define	TASK_STOPPED		2

#define TASK_NORMAL		(TASK_INTERRUPTIBLE | TASK_UNINTERRUPTIBLE)


/*
 * A task_struct is only provided for those tasks created with kthread.
 * Using these routines with threads not started via kthread will cause
 * panics because no task_struct is allocated and td_retval[1] is
 * overwritten by syscalls which kernel threads will not make use of.
 */

#define	current			task_struct_get(curthread)
#define	task_struct_get(x)	((struct task_struct *)(uintptr_t)(x)->td_lkpi_task)

#define	task_struct_fill(x, y) do {		\
  	(y)->task_thread = (x);			\
	(y)->comm = (x)->td_name;		\
	(y)->pid = (x)->td_tid;			\
	(y)->mm = &(y)->bsd_mm;			\
	(y)->usage.counter = 1;			\
	(y)->state = TASK_RUNNING;		\
} while (0)

struct wait_queue_head;

struct task_struct {
	struct	thread *task_thread;
	struct mm_struct *mm;
	int	(*task_fn)(void *data);
	atomic_t usage;
	void	*task_data;
	int	task_ret;
	int	state;
	int	should_stop;
	char	*comm;
	int	flags;
	pid_t	pid;
	struct wait_queue_head	*sleep_wq;
	void	*bsd_ioctl_data;
	unsigned	bsd_ioctl_len;
	struct mm_struct bsd_mm;
};


static inline void
linux_kthread_fn(void *arg)
{
	struct mm_struct *mm;
	struct task_struct *task;
	struct thread *td = curthread;

	task = arg;
	task_struct_fill(td, task);
	mm = task->mm;
	init_rwsem(&mm->mmap_sem);
	mm->mm_count.counter = 1;
	mm->mm_users.counter = 1;
	td->td_lkpi_task = task;
	if (task->should_stop == 0)
		task->task_ret = task->task_fn(task->task_data);
	PROC_LOCK(td->td_proc);
	task->should_stop = TASK_STOPPED;
	wakeup(task);
	PROC_UNLOCK(td->td_proc);
	td->td_lkpi_task = NULL;
	kthread_exit();
}

static inline struct task_struct *
linux_kthread_create(int (*threadfn)(void *data), void *data)
{
	struct task_struct *task;

	task = kzalloc(sizeof(*task), GFP_KERNEL);
	task->task_fn = threadfn;
	task->task_data = data;

	return (task);
}

#define	kthread_run(fn, data, fmt, ...)					\
({									\
	struct task_struct *_task;					\
									\
	_task = linux_kthread_create((fn), (data));			\
	if (kthread_add(linux_kthread_fn, _task, NULL, &_task->task_thread,	\
	    0, 0, fmt, ## __VA_ARGS__)) {				\
		kfree(_task);						\
		_task = NULL;						\
	}								\
	_task;								\
})

#define	kthread_should_stop()	current->should_stop

static inline int
wake_up_process(struct task_struct *p)
{
	int rc;

	rc = 0;
	if ((p->state & TASK_NORMAL) == 0)
		goto out;
	rc = 1;
	if (!TD_IS_RUNNING(p->task_thread)) {
		p->state = TASK_WAKING;
		wakeup_one(p);
	}
out:
	return (rc);
}

static inline int
kthread_stop(struct task_struct *task)
{

	PROC_LOCK(task->task_thread->td_proc);
	task->should_stop = TASK_SHOULD_STOP;
	wake_up_process(task);
	while (task->should_stop != TASK_STOPPED)
		msleep(task, &task->task_thread->td_proc->p_mtx, PWAIT,
		    "kstop", hz);
	PROC_UNLOCK(task->task_thread->td_proc);
	return task->task_ret;
}

extern int in_atomic(void);

#endif	/* _LINUX_KTHREAD_H_ */

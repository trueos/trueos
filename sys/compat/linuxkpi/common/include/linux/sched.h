/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2017 Mellanox Technologies, Ltd.
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
#ifndef	_LINUX_SCHED_H_
#define	_LINUX_SCHED_H_

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/sched.h>

#include <linux/list.h>
#include <linux/compat.h>
#include <linux/completion.h>
#include <linux/pid.h>
#include <linux/slab.h>
#include <linux/mm_types.h>
#include <linux/time64.h>
#include <linux/string.h>
#include <linux/bitmap.h>
#include <linux/atomic.h>
#include <linux/smp.h>

#include <asm/atomic.h>

#define	MAX_SCHEDULE_TIMEOUT	INT_MAX

#define	TASK_RUNNING		0
#define	TASK_INTERRUPTIBLE	1
#define	TASK_UNINTERRUPTIBLE	2
#define	TASK_KILLABLE           3
#define	TASK_DEAD		64
#define	TASK_WAKEKILL		128
#define	TASK_WAKING		256
#define	TASK_PARKED		512
#define	TASK_NORMAL		(TASK_INTERRUPTIBLE | TASK_UNINTERRUPTIBLE)

#define	TASK_COMM_LEN 16

struct seq_file;
struct wait_queue_head;

struct task_struct {
	struct thread *task_thread;
	struct mm_struct *mm;
	linux_task_fn_t *task_fn;
	void   *task_data;
	int	task_ret;
	atomic_t usage;
	atomic_t state;
	atomic_t kthread_flags;
	const char *comm;
	struct wait_queue_head *sleep_wq;
	pid_t	pid;	/* BSD thread ID */
	int	prio;
	int	static_prio;
	int	normal_prio;
	void   *bsd_ioctl_data;
	unsigned bsd_ioctl_len;
	struct mtx sleep_lock;
	struct completion parked;
	struct completion exited;
	TAILQ_ENTRY(task_struct) rcu_entry;
	int rcu_recurse;
};

#define	current	({ \
	struct thread *__td = curthread; \
	linux_set_current(__td); \
	((struct task_struct *)__td->td_lkpi_task); \
})

#define	task_pid_group_leader(task) (task)->task_thread->td_proc->p_pid
#define	task_pid(task)		((task)->pid)
#define	task_pid_nr(task)	((task)->pid)
#define	get_pid(x)		(x)
#define	put_pid(x)		do { } while (0)
#define	current_euid()	(curthread->td_ucred->cr_uid)

#define	set_current_state(x)	set_task_state(current, x)
#define	__set_current_state(x)	__set_task_state(current, x)

#define	set_task_state(task, x) atomic_set(&(task)->state, x)
#define	__set_task_state(task, x) do { (task)->state.counter = (x); } while (0)

static inline void
get_task_struct(struct task_struct *task)
{
	atomic_inc(&task->usage);
}

static inline void
put_task_struct(struct task_struct *task)
{
	if (atomic_dec_and_test(&task->usage))
		linux_free_current(task);
}

extern u64 cpu_clock(int cpu);
extern u64 running_clock(void);
extern u64 sched_clock_cpu(int cpu);

static inline u64
local_clock(void)
{
	struct timespec ts;

	nanotime(&ts);
	return (ts.tv_sec * NSEC_PER_SEC) + ts.tv_nsec;
}

#define	cond_resched()	if (!cold)	sched_relinquish(curthread)

#define	sched_yield()	sched_relinquish(curthread)


static inline int
send_sig(int signo, struct task_struct *t, int priv)
{
	/* Only support signalling current process right now  */
	MPASS(t == current);

	PROC_LOCK(curproc);
	tdsignal(curthread, signo);
	PROC_UNLOCK(curproc);
	return (0);
}

static inline int
signal_pending(struct task_struct *p)
{
	return SIGPENDING(p->task_thread);
}

static inline int
__fatal_signal_pending(struct task_struct *p)
{
	return (SIGISMEMBER(p->task_thread->td_siglist, SIGKILL));
}

static inline int
fatal_signal_pending(struct task_struct *p)
{
	return signal_pending(p) && __fatal_signal_pending(p);
}

static inline int
signal_pending_state(long state, struct task_struct *p)
{
	if (!(state & (TASK_INTERRUPTIBLE | TASK_WAKEKILL)))
		return 0;
	if (!signal_pending(p))
		return 0;

	return (state & TASK_INTERRUPTIBLE) || __fatal_signal_pending(p);
}

extern long schedule_timeout(long timeout);

static inline long
schedule_timeout_uninterruptible(long timeout)
{
	MPASS(current);
	__set_current_state(TASK_UNINTERRUPTIBLE);
	return (schedule_timeout(timeout));
}

static inline long
schedule_timeout_interruptible(long timeout)
{
	MPASS(current);
	__set_current_state(TASK_INTERRUPTIBLE);
	return (schedule_timeout(timeout));
}

static inline long
schedule_timeout_killable(long timeout)
{
	return (schedule_timeout(timeout));
}

static inline long
io_schedule_timeout(long timeout)
{
	return (schedule_timeout(timeout));
}

static inline void
io_schedule(void)
{
	io_schedule_timeout(MAX_SCHEDULE_TIMEOUT);
}

static inline void
schedule(void)
{
	schedule_timeout(MAX_SCHEDULE_TIMEOUT);
}

#define	yield() kern_yield(PRI_UNCHANGED)

#define	need_resched() (curthread->td_flags & TDF_NEEDRESCHED)

#endif	/* _LINUX_SCHED_H_ */

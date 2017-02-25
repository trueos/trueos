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
#ifndef	_LINUX_WAIT_H_
#define	_LINUX_WAIT_H_

#include <linux/compiler.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/srcu.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/selinfo.h>

#define	SKIP_SLEEP() (SCHEDULER_STOPPED() || kdb_active)

static inline void
wake_up_atomic_t(atomic_t *v)
{
	panic("%s unimplemented", __FUNCTION__);
}

static inline int
wait_on_atomic_t(atomic_t *val, int (*action) (atomic_t *), unsigned mode)
{
	panic("%s unimplemented", __FUNCTION__);
}

struct wait_queue;
struct wait_queue_head;
struct wait_bit_key;
struct wait_bit_queue;

typedef struct wait_queue wait_queue_t;
typedef struct wait_queue_head wait_queue_head_t;
typedef struct wait_bit_queue wait_bit_queue_t;

typedef int wait_queue_func_t (wait_queue_t *, unsigned mode, int flags, void *key);
typedef void wake_up_func_t (wait_queue_head_t *, unsigned mode, int flags, void *key);

struct wait_queue {
	unsigned flags;
#define	WQ_FLAG_EXCLUSIVE	0x01
#define	WQ_FLAG_WOKEN		0x02
	void   *private;
	wait_queue_func_t *func;
	struct list_head task_list;
};

struct wait_bit_key {
	void   *flags;
	int	bit_nr;
#define	WAIT_ATOMIC_T_BIT_NR	-1
	int	timeout;
};

struct wait_bit_queue {
	struct wait_bit_key key;
	wait_queue_t wait;
};

struct wait_queue_head {
	spinlock_t lock;
	struct list_head task_list;
	struct selinfo wqh_si;
	struct list_head wqh_file_list;
};

#define	LINUX_WAIT_BIT_KEY_INITIALIZER(word, bit, to) {	\
	.flags = (word),				\
	.bit_nr = (bit),				\
	.timeout = (to),				\
}

#define	DEFINE_WAIT_FUNC(name, function)			\
wait_queue_t name = {						\
	.private	= current,				\
	.func		= function,				\
	.task_list	= LINUX_LIST_HEAD_INIT(name.task_list),	\
}

#define	DEFINE_WAIT(name) \
	DEFINE_WAIT_FUNC(name, autoremove_wake_function)

#define	LINUX_DEFINE_WAIT_BIT(name, word, bit, to)		\
wait_bit_queue_t name = {					\
	.key = LINUX_WAIT_BIT_KEY_INITIALIZER(word, bit, to),	\
	.wait = {						\
		.private	= current,			\
		.func		= wake_bit_function,		\
		.task_list	=				\
		    LINUX_LIST_HEAD_INIT(name.wait.task_list),	\
	},							\
}

#define	init_wait(wait) do {				\
	(wait)->private = current;			\
	(wait)->func = autoremove_wake_function;	\
	INIT_LIST_HEAD(&(wait)->task_list);		\
	(wait)->flags = 0;				\
} while (0)

extern wait_queue_func_t default_wake_function;
extern wait_queue_func_t autoremove_wake_function;
extern wait_queue_func_t wake_bit_function;

extern void linux_abort_exclusive_wait(wait_queue_head_t *, wait_queue_t *,
    unsigned mode, void *key);
extern wake_up_func_t linux_wake_up_locked;
extern wake_up_func_t linux_wake_up;

#define	LINUX_WAITQUEUE_INITIALIZER(name, task) {	\
	.private	= task,				\
	.func		= default_wake_function,	\
	.task_list	= { NULL, NULL },		\
}

#define	DECLARE_WAITQUEUE(name, task)			\
	wait_queue_t name =  LINUX_WAITQUEUE_INITIALIZER(name, task)

#define	LINUX_WAIT_QUEUE_HEAD_INITIALIZER(name) {	\
	.task_list = {					\
		&(name).task_list,			\
		&(name).task_list,			\
	},						\
	.wqh_file_list = {				\
		&(name).wqh_file_list,			\
		&(name).wqh_file_list,			\
	},						\
}

#define	DECLARE_WAIT_QUEUE_HEAD(name)			\
	wait_queue_head_t name =			\
	    LINUX_WAIT_QUEUE_HEAD_INITIALIZER(name);	\
	MTX_SYSINIT(name, &(name).lock.m,		\
	    spin_lock_name("wqhead"), MTX_DEF)

static inline void
init_waitqueue_head(wait_queue_head_t *wh)
{

	memset(wh, 0, sizeof(*wh));
	mtx_init(&wh->lock.m, spin_lock_name("wqhead"),
	    NULL, MTX_DEF | MTX_NOWITNESS);
	INIT_LIST_HEAD(&wh->task_list);
	INIT_LIST_HEAD(&wh->wqh_file_list);
}

static inline void
init_waitqueue_entry(wait_queue_t *wq, struct task_struct *p)
{
	wq->flags = 0;
	wq->private = p;
	wq->func = default_wake_function;
}

static inline void
init_waitqueue_func_entry(wait_queue_t *wq, wait_queue_func_t *func)
{
	wq->flags = 0;
	wq->private = NULL;
	wq->func = func;
}

#define	wake_up(q) \
	linux_wake_up(q, TASK_NORMAL, 1, NULL)
#define	wake_up_nr(q, nr) \
	linux_wake_up(q, TASK_NORMAL, nr, NULL)
#define	wake_up_all(q) \
	linux_wake_up(q, TASK_NORMAL, 0, NULL)
#define	wake_up_locked(q) \
	linux_wake_up_locked(q, TASK_NORMAL, 1, NULL)
#define	wake_up_all_locked(q) \
	linux_wake_up_locked(q, TASK_NORMAL, 0, NULL)
#define	wake_up_interruptible(q) \
	linux_wake_up(q, TASK_INTERRUPTIBLE, 1, NULL)
#define	wake_up_interruptible_nr(q, nr) \
	linux_wake_up(q, TASK_INTERRUPTIBLE, nr, NULL)
#define	wake_up_interruptible_all(q) \
	linux_wake_up(q, TASK_INTERRUPTIBLE, 0, NULL)
#define	linux_wake_up_locked_key(q, mode, key)	\
	linux_wake_up_locked(q, mode, 0, key)

#define	might_sleep() \
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL, "might_sleep()")

#define	___wait_cond_timeout(ret, condition) ({	\
	bool __cond = (condition);		\
	if (__cond && !ret)			\
		ret = 1;			\
	__cond || !ret;				\
})

#define	___wait_is_interruptible(state) (	\
	__builtin_constant_p(state) == 0 ||	\
	(state) == TASK_INTERRUPTIBLE ||	\
	(state) == TASK_KILLABLE		\
)

#define	___wait_event(wq, condition, state, exclusive, ret, cmd) ({	\
	__label__ __out;						\
	wait_queue_t __wait;						\
	int __ret = (ret);						\
									\
	INIT_LIST_HEAD(&__wait.task_list);				\
	if (exclusive)							\
		__wait.flags = WQ_FLAG_EXCLUSIVE;			\
	else								\
		__wait.flags = 0;					\
									\
	for (;;) {							\
		int __int = prepare_to_wait_event(&wq, &__wait, state); \
									\
		if (condition)						\
			break;						\
									\
		if (___wait_is_interruptible(state) && __int) {		\
			__ret = __int;					\
			if (exclusive) {				\
				linux_abort_exclusive_wait(		\
				    &wq, &__wait, state, NULL);		\
				goto __out;				\
			}						\
			break;						\
		}							\
		cmd;							\
	}								\
	finish_wait(&wq, &__wait);					\
__out:	__ret;								\
})

#define	__wait_event(wq, condition)					\
	(void)___wait_event(wq, condition, TASK_UNINTERRUPTIBLE, 0, 0,	\
			    schedule())

#define	wait_event(wq, condition)					\
do {									\
	might_sleep();							\
	if (condition)							\
		break;							\
	__wait_event(wq, condition);					\
} while (0)


#define	io_wait_event(wq, condition)					\
do {									\
	might_sleep();							\
	if (condition)							\
		break;							\
	__io_wait_event(wq, condition);					\
} while (0)


#define	__wait_event_timeout(wq, condition, timeout)			\
	___wait_event(wq, ___wait_cond_timeout(__ret, condition),	\
	    TASK_UNINTERRUPTIBLE, 0, timeout,				\
	    __ret = schedule_timeout(__ret))

#define	wait_event_timeout(wq, condition, timeout) ({			\
	int __ret = (timeout);						\
	might_sleep();							\
	if (!___wait_cond_timeout(__ret, condition))			\
		__ret = __wait_event_timeout(wq, condition, timeout);	\
	__ret;								\
})

#define	__wait_event_interruptible(wq, condition)			\
	___wait_event(wq, condition, TASK_INTERRUPTIBLE, 0, 0,		\
		      schedule())

#define	wait_event_interruptible(wq, condition) ({			\
	int __ret = 0;							\
	might_sleep();							\
	if (!(condition))						\
		__ret = __wait_event_interruptible(wq, condition);	\
	__ret;								\
})

#define	__wait_event_interruptible_timeout(wq, condition, timeout)	\
	___wait_event(wq, ___wait_cond_timeout(__ret, condition),	\
		      TASK_INTERRUPTIBLE, 0, timeout,			\
		      __ret = schedule_timeout(__ret))

#define	wait_event_interruptible_timeout(wq, condition, timeout) ({	\
	int __ret = (timeout);						\
	might_sleep();							\
	if (!___wait_cond_timeout(__ret, condition)) {			\
		__ret = __wait_event_interruptible_timeout(wq,		\
		    condition, timeout);				\
	}								\
	__ret;								\
})

#define	__wait_event_interruptible_locked(wq, cond, exclusive, irq) ({	\
	int __ret = 0;							\
	DEFINE_WAIT(__wait);						\
	if (exclusive)							\
		__wait.flags |= WQ_FLAG_EXCLUSIVE;			\
	do {								\
		if (likely(list_empty(&__wait.task_list)))		\
			__add_wait_queue_tail(&(wq), &__wait);		\
		set_current_state(TASK_INTERRUPTIBLE);			\
		if (signal_pending(current)) {				\
			__ret = -ERESTARTSYS;				\
			break;						\
		}							\
		if (irq)						\
			spin_unlock_irq(&(wq).lock);			\
		else							\
			spin_unlock(&(wq).lock);			\
		schedule();						\
		if (irq)						\
			spin_lock_irq(&(wq).lock);			\
		else							\
			spin_lock(&(wq).lock);				\
	} while (!(cond));						\
	__remove_wait_queue(&(wq), &__wait);				\
	__set_current_state(TASK_RUNNING);				\
	__ret;								\
})

#define	wait_event_interruptible_locked(wq, cond)			\
	((cond) ? 0 : __wait_event_interruptible_locked(wq, cond, 0, 0))

#define	__wait_event_interruptible_lock_irq(wq, condition, lock, cmd)	\
	___wait_event(wq, condition, TASK_INTERRUPTIBLE, 0, 0,		\
	    spin_unlock_irq(&lock);					\
	    cmd;							\
	    schedule();							\
	    spin_lock_irq(&lock))

#define	wait_event_interruptible_lock_irq(wq, condition, lock) ({	\
	int __ret = 0;							\
	if (SKIP_SLEEP() == 0 && (condition) == 0) {			\
		__ret = __wait_event_interruptible_lock_irq(		\
		    wq, condition, lock,);				\
	}								\
	__ret;								\
})

static inline void
__add_wait_queue(wait_queue_head_t *head, wait_queue_t *new)
{
	list_add(&new->task_list, &head->task_list);
}

static inline void
add_wait_queue(wait_queue_head_t *wqh, wait_queue_t *wait)
{

	wait->flags &= ~WQ_FLAG_EXCLUSIVE;
	spin_lock(&wqh->lock);
	__add_wait_queue(wqh, wait);
	spin_unlock(&wqh->lock);
}

static inline void
__add_wait_queue_tail(wait_queue_head_t *head, wait_queue_t *new)
{
	list_add_tail(&new->task_list, &head->task_list);
}

static inline void
__remove_wait_queue(wait_queue_head_t *head, wait_queue_t *old)
{
	list_del(&old->task_list);
}

static inline void
remove_wait_queue(wait_queue_head_t *wqh, wait_queue_t *wait)
{

	spin_lock(&wqh->lock);
	__remove_wait_queue(wqh, wait);
	spin_unlock(&wqh->lock);
}

static inline int
waitqueue_active(wait_queue_head_t *wqh)
{
	return (!list_empty(&wqh->task_list));
}

static inline void
prepare_to_wait_exclusive(wait_queue_head_t *wqh, wait_queue_t *wait, int state)
{

	wait->flags |= WQ_FLAG_EXCLUSIVE;
	spin_lock(&wqh->lock);
	if (list_empty(&wait->task_list))
		__add_wait_queue_tail(wqh, wait);
	set_current_state(state);
	spin_unlock(&wqh->lock);
}

static inline long
prepare_to_wait_event(wait_queue_head_t *wqh, wait_queue_t *wait, int state)
{
	struct task_struct *task = current;

	if (signal_pending_state(state, task))
		return (-ERESTARTSYS);

	task->sleep_wq = wqh;
	wait->private = task;
	wait->func = autoremove_wake_function;

	spin_lock(&wqh->lock);
	if (list_empty(&wait->task_list)) {
		if (wait->flags & WQ_FLAG_EXCLUSIVE)
			__add_wait_queue_tail(wqh, wait);
		else
			__add_wait_queue(wqh, wait);
	}
	set_task_state(task, state);
	spin_unlock(&wqh->lock);

	return (0);
}

static inline void
prepare_to_wait(wait_queue_head_t *wqh, wait_queue_t *wait, int state)
{
	struct task_struct *task = current;

	MPASS(task != NULL);

	task->sleep_wq = wqh;
	spin_lock(&wqh->lock);
	if (list_empty(&wait->task_list))
		__add_wait_queue(wqh, wait);
	set_task_state(task, state);
	spin_unlock(&wqh->lock);
}

static inline void
finish_wait(wait_queue_head_t *wqh, wait_queue_t *wait)
{
	struct task_struct *task = current;

	MPASS(task != NULL);
	MPASS(task->sleep_wq == wqh);

	task->sleep_wq = NULL;
	__set_task_state(task, TASK_RUNNING);

	if (!list_empty_careful(&wait->task_list)) {
		spin_lock(&wqh->lock);
		list_del_init(&wait->task_list);
		spin_unlock(&wqh->lock);
	}
}

extern wait_queue_head_t linux_bit_wait_queue_head;
extern int linux_wait_on_bit_timeout(wait_queue_head_t *, struct wait_bit_queue *,
    unsigned mode);

static inline void
wake_up_bit(void *word, int bit)
{
	wait_queue_head_t *wq = &linux_bit_wait_queue_head;

	if (waitqueue_active(wq)) {
		struct wait_bit_key key =
		    LINUX_WAIT_BIT_KEY_INITIALIZER(word, bit, 0);

		linux_wake_up(wq, TASK_NORMAL, 1, &key);
	}
}

static inline int
wait_on_bit_timeout(unsigned long *word, int bit, unsigned mode,
    int timeout)
{
	might_sleep();

	if (test_bit(bit, word)) {
		wait_queue_head_t *wq = &linux_bit_wait_queue_head;
		LINUX_DEFINE_WAIT_BIT(wait, word, bit, jiffies + timeout);

		return (linux_wait_on_bit_timeout(wq, &wait, mode));
	}
	return (0);
}

#endif					/* _LINUX_WAIT_H_ */

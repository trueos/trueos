/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
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
#include <sys/sleepqueue.h>
#include <sys/kernel.h>
#include <sys/proc.h>

#define SKIP_SLEEP() (SCHEDULER_STOPPED() || kdb_active)

static inline void
wake_up_atomic_t(atomic_t *v)
{
	panic("%s unimplemented", __FUNCTION__);
}

static inline int
wait_on_atomic_t(atomic_t *val, int (*action)(atomic_t *), unsigned mode)
{
	panic("%s unimplemented", __FUNCTION__);
}

struct __wait_queue;
typedef struct __wait_queue wait_queue_t;

typedef int (*wait_queue_func_t)(wait_queue_t *wait, unsigned mode, int flags, void *key);


#define WQ_FLAG_EXCLUSIVE	0x01
#define WQ_FLAG_WOKEN		0x02

struct __wait_queue {
	unsigned int		flags;
	void			*private;
	wait_queue_func_t	func;
	struct list_head	task_list;
};

struct wait_bit_key {
	void			*flags;
	int			bit_nr;
#define WAIT_ATOMIC_T_BIT_NR	-1
	unsigned long		timeout;
};

struct wait_bit_queue {
	struct wait_bit_key	key;
	wait_queue_t		wait;
};

typedef struct wait_queue_head {
	spinlock_t	lock;
	struct list_head	task_list;
	struct selinfo		wqh_si;
	struct list_head	wqh_file_list;
} wait_queue_head_t;


#define __WAIT_BIT_KEY_INITIALIZER(word, bit)				\
	{ .flags = word, .bit_nr = bit, }

static inline int
default_wake_function(wait_queue_t *curr, unsigned mode, int wake_flags,
			  void *key)
{
	return (linux_try_to_wake_up(curr->private, mode));
}

static inline int
autoremove_wake_function(wait_queue_t *wait, unsigned mode, int sync, void *key)
{
	int ret = default_wake_function(wait, mode, sync, key);

	if (ret)
		list_del_init(&wait->task_list);
	return ret;
}

int wake_bit_function(wait_queue_t *wait, unsigned mode, int sync, void *key);

#define DEFINE_WAIT_FUNC(name, function)				\
	wait_queue_t name = {						\
		.private	= current,				\
		.func		= function,				\
		.task_list	= LINUX_LIST_HEAD_INIT((name).task_list),	\
	}

#define DEFINE_WAIT(name) DEFINE_WAIT_FUNC(name, autoremove_wake_function)

#define DEFINE_WAIT_BIT(name, word, bit)				\
	struct wait_bit_queue name = {					\
		.key = __WAIT_BIT_KEY_INITIALIZER(word, bit),		\
		.wait	= {						\
			.private	= current,			\
			.func		= wake_bit_function,		\
			.task_list	=				\
				LINUX_LIST_HEAD_INIT((name).wait.task_list),	\
		},							\
	}

#define init_wait(wait)							\
	do {								\
		(wait)->private = current;				\
		(wait)->func = autoremove_wake_function;		\
		INIT_LIST_HEAD(&(wait)->task_list);			\
		(wait)->flags = 0;					\
	} while (0)


extern int bit_wait(struct wait_bit_key *, int);
extern int bit_wait_io(struct wait_bit_key *, int);
extern int bit_wait_timeout(struct wait_bit_key *, int);
extern int bit_wait_io_timeout(struct wait_bit_key *, int);


#define LINUX_WAITQUEUE_INITIALIZER(name, tsk) {			\
	.private	= tsk,						\
 	.func		= default_wake_function,	                \
	.task_list	= { NULL, NULL } }

#define DECLARE_WAITQUEUE(name, tsk)				\
	wait_queue_t name =  LINUX_WAITQUEUE_INITIALIZER(name, tsk)

#define LINUX_WAIT_QUEUE_HEAD_INITIALIZER(name) {			\
	.task_list	= { &(name).task_list, &(name).task_list }, \
	.wqh_file_list = { &(name).wqh_file_list, &(name).wqh_file_list }}


#define DECLARE_WAIT_QUEUE_HEAD(name)		\
	wait_queue_head_t name = LINUX_WAIT_QUEUE_HEAD_INITIALIZER(name); \
	MTX_SYSINIT(name, &(name).lock.m, spin_lock_name("wqhead"), MTX_DEF)

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
	wq->flags	= 0;
	wq->private	= p;
	wq->func	= default_wake_function;
}

static inline void
init_waitqueue_func_entry(wait_queue_t *wq, wait_queue_func_t func)
{
	wq->flags	= 0;
	wq->private	= NULL;
	wq->func	= func;
}

static inline void
__wake_up_locked(wait_queue_head_t *q, int mode, int nr, void *key)
{
	wait_queue_t *curr;
	wait_queue_t *next;

	list_for_each_entry_safe(curr, next, &q->task_list, task_list) {
		unsigned flags = curr->flags;

		if (curr->func(curr, TASK_NORMAL, 0, key) &&
		    (flags & WQ_FLAG_EXCLUSIVE) && !--nr)
			break;
	}
}

typedef int wait_bit_action_f(struct wait_bit_key *, int mode);
void __wake_up(wait_queue_head_t *q, int mode, int nr, void *key);


#define	wake_up(q)				__wake_up(q, TASK_NORMAL, 1, NULL)
#define	wake_up_nr(q, nr)			__wake_up(q, TASK_NORMAL, nr, NULL)
#define	wake_up_all(q)				__wake_up(q, TASK_NORMAL, 0, NULL)
#define	wake_up_locked(q)			__wake_up_locked(q, TASK_NORMAL, 1, NULL)
#define	wake_up_all_locked(q)			__wake_up_locked(q, TASK_NORMAL, 0, NULL)
#define	wake_up_interruptible(q)		__wake_up(q, TASK_INTERRUPTIBLE, 1, NULL)
#define	wake_up_interruptible_nr(q, nr)		__wake_up(q, TASK_INTERRUPTIBLE, nr, NULL)
#define	wake_up_interruptible_all(q)		__wake_up(q, TASK_INTERRUPTIBLE, 0, NULL)

#define __wake_up_locked_key(q, mode, key)	__wake_up_locked(q, mode, 0, key)

#define	might_sleep() \
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL, "might_sleep()")

#define ___wait_cond_timeout(condition)					\
({									\
	bool __cond = (condition);					\
	if (__cond && !__ret)						\
		__ret = 1;						\
	__cond || !__ret;						\
})

#define ___wait_is_interruptible(state)					\
	(!__builtin_constant_p(state) ||				\
		state == TASK_INTERRUPTIBLE || state == TASK_KILLABLE)	\

#define ___wait_event(wq, condition, state, exclusive, ret, cmd)	\
({									\
	__label__ __out;						\
	wait_queue_t __wait;						\
	long __ret = ret;	/* explicit shadow */			\
									\
	INIT_LIST_HEAD(&__wait.task_list);				\
	if (exclusive)							\
		__wait.flags = WQ_FLAG_EXCLUSIVE;			\
	else								\
		__wait.flags = 0;					\
									\
	for (;;) {							\
		long __int = prepare_to_wait_event(&wq, &__wait, state);\
									\
		if (condition)						\
			break;						\
									\
		if (___wait_is_interruptible(state) && __int) {		\
			__ret = __int;					\
			if (exclusive) {				\
				abort_exclusive_wait(&wq, &__wait,	\
						     state, NULL);	\
				goto __out;				\
			}						\
			break;						\
		}							\
									\
		cmd;							\
	}								\
	finish_wait(&wq, &__wait);					\
__out:	__ret;								\
})

#define __wait_event(wq, condition)					\
	(void)___wait_event(wq, condition, TASK_UNINTERRUPTIBLE, 0, 0,	\
			    schedule())

#define wait_event(wq, condition)					\
do {									\
	might_sleep();							\
	if (condition)							\
		break;							\
	__wait_event(wq, condition);					\
} while (0)


#define io_wait_event(wq, condition)					\
do {									\
	might_sleep();							\
	if (condition)							\
		break;							\
	__io_wait_event(wq, condition);					\
} while (0)


#define __wait_event_timeout(wq, condition, timeout)			\
	___wait_event(wq, ___wait_cond_timeout(condition),		\
		      TASK_UNINTERRUPTIBLE, 0, timeout,			\
		      __ret = schedule_timeout(__ret))

#define wait_event_timeout(wq, condition, timeout)			\
({									\
	long __ret = timeout;						\
	might_sleep();							\
	if (!___wait_cond_timeout(condition))				\
		__ret = __wait_event_timeout(wq, condition, timeout);	\
	__ret;								\
})


/*
 * XXX - work around lost wakeups to AMD's gpu scheduler by waking up every
 * 100ms
 */
#define __wait_event_interruptible(wq, condition)			\
	___wait_event(wq, condition, TASK_INTERRUPTIBLE, 0, 0,		\
		      schedule_short())

#define wait_event_interruptible(wq, condition)				\
({									\
	int __ret = 0;							\
	might_sleep();							\
	if (!(condition))						\
		__ret = __wait_event_interruptible(wq, condition);	\
	__ret;								\
})



#define __wait_event_interruptible_timeout(wq, condition, timeout)	\
	___wait_event(wq, ___wait_cond_timeout(condition),		\
		      TASK_INTERRUPTIBLE, 0, timeout,			\
		      __ret = schedule_timeout(__ret))



#define wait_event_interruptible_timeout(wq, condition, timeout)	\
({									\
	long __ret = timeout;						\
	might_sleep();							\
	if (!___wait_cond_timeout(condition))				\
		__ret = __wait_event_interruptible_timeout(wq,		\
						condition, timeout);	\
	__ret;								\
})


#define __wait_event_interruptible_locked(wq, condition, exclusive, irq) \
({									\
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
		schedule();					\
		if (irq)						\
			spin_lock_irq(&(wq).lock);			\
		else							\
			spin_lock(&(wq).lock);				\
	} while (!(condition));						\
	__remove_wait_queue(&(wq), &__wait);				\
	__set_current_state(TASK_RUNNING);				\
	__ret;								\
})


#define wait_event_interruptible_locked(wq, condition)			\
	((condition)							\
	 ? 0 : __wait_event_interruptible_locked(wq, condition, 0, 0))


#define __wait_event_interruptible_lock_irq(wq, condition, lock, cmd) ({panic("implement me!!! XXX"); 0;})
#if 0
___wait_event(wq, condition, TASK_INTERRUPTIBLE, 0, 0,			\
		      spin_unlock_irq(&lock);				\
		      cmd;						\
		      schedule();					\
		      spin_lock_irq(&lock))
#endif

#define wait_event_interruptible_lock_irq(wq, condition, lock)		\
({									\
	int __ret = 0;							\
	if (SKIP_SLEEP())						\
		goto done;						\
	if (!(condition))						\
		__ret = __wait_event_interruptible_lock_irq(wq,		\
						condition, lock,);	\
done:									\
	__ret;								\
})


static inline void
__add_wait_queue(wait_queue_head_t *head, wait_queue_t *new)
{
	list_add(&new->task_list, &head->task_list);
}

static inline void
add_wait_queue(wait_queue_head_t *q, wait_queue_t *wait)
{
	unsigned long flags;

	wait->flags &= ~WQ_FLAG_EXCLUSIVE;
	spin_lock_irqsave(&q->lock, flags);
	__add_wait_queue(q, wait);
	spin_unlock_irqrestore(&q->lock, flags);
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
remove_wait_queue(wait_queue_head_t *q, wait_queue_t *wait)
{
	unsigned long flags;

	spin_lock_irqsave(&q->lock, flags);
	__remove_wait_queue(q, wait);
	spin_unlock_irqrestore(&q->lock, flags);
}

static inline int
waitqueue_active(wait_queue_head_t *q)
{
	return (!list_empty(&q->task_list));
}


static inline void
abort_exclusive_wait(wait_queue_head_t *q, wait_queue_t *wait,
			unsigned int mode, void *key)
{
	unsigned long flags;

	__set_current_state(TASK_RUNNING);
	spin_lock_irqsave(&q->lock, flags);
	if (!list_empty(&wait->task_list))
		list_del_init(&wait->task_list);
	else if (waitqueue_active(q))
		__wake_up_locked_key(q, mode, key);
	spin_unlock_irqrestore(&q->lock, flags);
}

static inline void
prepare_to_wait_exclusive(wait_queue_head_t *q, wait_queue_t *wait, int state)
{
	unsigned long flags;

	wait->flags |= WQ_FLAG_EXCLUSIVE;
	spin_lock_irqsave(&q->lock, flags);
	if (list_empty(&wait->task_list))
		__add_wait_queue_tail(q, wait);
	set_current_state(state);
	spin_unlock_irqrestore(&q->lock, flags);
}

static inline long
prepare_to_wait_event(wait_queue_head_t *q, wait_queue_t *wait, int state)
{
	unsigned long flags;

	if (signal_pending_state(state, current))
		return -ERESTARTSYS;

	current->sleep_wq = q;	
	wait->private = current;
	wait->func = autoremove_wake_function;

	spin_lock_irqsave(&q->lock, flags);
	if (list_empty(&wait->task_list)) {
		if (wait->flags & WQ_FLAG_EXCLUSIVE)
			__add_wait_queue_tail(q, wait);
		else
			__add_wait_queue(q, wait);
	}
	set_current_state(state);
	spin_unlock_irqrestore(&q->lock, flags);

	return 0;
}

static inline void
prepare_to_wait(wait_queue_head_t *q, wait_queue_t *wait, int state)
{
	int flags;
	MPASS(current != NULL);

	current->sleep_wq = q;
	spin_lock_irqsave(&q->lock, flags);
	if (list_empty(&wait->task_list))
		__add_wait_queue(q, wait);
	set_current_state(state);
	spin_unlock_irqrestore(&q->lock, flags);
}

static inline void
finish_wait(wait_queue_head_t *q, wait_queue_t *wait)
{
	int flags;
	MPASS(current != NULL);
	MPASS(current->sleep_wq == q);

	current->sleep_wq = NULL;
	__set_current_state(TASK_RUNNING);

	if (!list_empty_careful(&wait->task_list)) {
		spin_lock_irqsave(&q->lock, flags);
		list_del_init(&wait->task_list);
		spin_unlock_irqrestore(&q->lock, flags);
	}
}

wait_queue_head_t *bit_waitqueue(void *word, int bit);
/*
 * Linux re-discovers sleep channels and re-implements them 
 * in the most irritating, hacked up way possible
 */

static inline int
__wait_on_bit(wait_queue_head_t *wq, struct wait_bit_queue *q,
	      wait_bit_action_f *action, unsigned mode)
{
	int ret = 0;

	do {
		prepare_to_wait(wq, &q->wait, mode);
		if (test_bit(q->key.bit_nr, q->key.flags))
			ret = (*action)(&q->key, mode);
	} while (test_bit(q->key.bit_nr, q->key.flags) && !ret);
	finish_wait(wq, &q->wait);

	return (ret);
}
static inline void
__wake_up_bit(wait_queue_head_t *wq, void *word, int bit)
{
	struct wait_bit_key key = __WAIT_BIT_KEY_INITIALIZER(word, bit);
	if (waitqueue_active(wq))
		__wake_up(wq, TASK_NORMAL, 1, &key);
}

static inline void
wake_up_bit(void *word, int bit)
{
	__wake_up_bit(bit_waitqueue(word, bit), word, bit);
}

static inline int
out_of_line_wait_on_bit_timeout(
	void *word, int bit, wait_bit_action_f *action,
	unsigned mode, unsigned long timeout)
{
	wait_queue_head_t *wq = bit_waitqueue(word, bit);
	DEFINE_WAIT_BIT(wait, word, bit);

	wait.key.timeout = jiffies + timeout;
	return __wait_on_bit(wq, &wait, action, mode);
}

static inline int
wait_on_bit_timeout(unsigned long *word, int bit, unsigned mode,
		    unsigned long timeout)
{
	might_sleep();
	if (!test_bit(bit, word))
		return 0;
	return out_of_line_wait_on_bit_timeout(word, bit,
					       bit_wait_timeout,
					       mode, timeout);
}

#endif

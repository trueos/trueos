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

#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/jiffies.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sleepqueue.h>
#include <sys/kernel.h>
#include <sys/proc.h>

typedef struct {
} wait_queue_t;

typedef struct {
	spinlock_t	lock;
	unsigned int	wchan;
} wait_queue_head_t;

#define DECLARE_WAITQUEUE(name, tsk)	\
	wait_queue_t name
#define DECLARE_WAIT_QUEUE_HEAD(name)    \
        wait_queue_head_t name

#define	init_waitqueue_head(x) \
    do { } while (0)

static inline void
__wake_up(wait_queue_head_t *q, int all)
{

	if (all == 0)
		wakeup_one(&q->wchan);
	else
		wakeup(&q->wchan);
}

#define	wake_up(q)				__wake_up(q, 0)
#define	wake_up_nr(q, nr)			__wake_up(q, 1)
#define	wake_up_all(q)				__wake_up(q, 1)
#define	wake_up_interruptible(q)		__wake_up(q, 0)
#define	wake_up_interruptible_nr(q, nr)		__wake_up(q, 1)
#define	wake_up_interruptible_all(q)		__wake_up(q, 1)

#define	wait_event(q, cond)						\
do {									\
	void *c = &(q).wchan;						\
	if (!(cond)) {							\
		for (;;) {						\
			sleepq_lock(c);					\
			if (cond) {					\
				sleepq_release(c);			\
				break;					\
			}						\
			sleepq_add(c, NULL, "completion", SLEEPQ_SLEEP, 0); \
			sleepq_wait(c, 0);				\
		}							\
	}								\
} while (0)

#define	wait_event_interruptible(q, cond)				\
({									\
	void *c = &(q).wchan;						\
	int _error;							\
									\
	_error = 0;							\
	if (!(cond)) {							\
		for (; _error == 0;) {					\
			sleepq_lock(c);					\
			if (cond) {					\
				sleepq_release(c);			\
				break;					\
			}						\
			sleepq_add(c, NULL, "completion",		\
			    SLEEPQ_SLEEP | SLEEPQ_INTERRUPTIBLE, 0);	\
			if (sleepq_wait_sig(c, 0))			\
				_error = -ERESTARTSYS;			\
		}							\
	}								\
	-_error;							\
})

/* jiffies2ticks XXX ? */
#define	wait_event_interruptible_timeout(q, cond, timeout)		\
({									\
	void *c = &(q).wchan;						\
	int _error;							\
									\
	_error = 0;							\
	if (!(cond)) {							\
		for (; _error == 0;) {					\
			sleepq_lock(c);					\
			if (cond) {					\
				sleepq_release(c);			\
				break;					\
			}						\
			sleepq_add(c, NULL, "completion",		\
				   SLEEPQ_SLEEP | SLEEPQ_INTERRUPTIBLE, 0); \
			sleepq_set_timeout(c, timeout);			\
			if (sleepq_timedwait_sig(c, 0))			\
				_error = -ERESTARTSYS;			\
		}							\
	}								\
	-_error;							\
})


#define	__wait_event_timeout(q, cond, timeout)				\
({									\
	void *c = &(q).wchan;						\
	int _error;							\
									\
	_error = 0;							\
	if (!(cond)) {							\
		for (; _error == 0;) {					\
			sleepq_lock(c);					\
			if (cond) {					\
				sleepq_release(c);			\
				break;					\
			}						\
			sleepq_add(c, NULL, "completion",		\
				   SLEEPQ_SLEEP, 0); \
			sleepq_set_timeout(c, timeout);			\
			if (sleepq_timedwait(c, 0))			\
				_error = -ERESTARTSYS;			\
		}							\
	}								\
	-_error;							\
})



#define wait_event_timeout(wq, condition, timeout)			\
({									\
	long __ret = timeout;						\
	if (!(condition)) 						\
		__wait_event_timeout(wq, condition, __ret);		\
 	__ret;								\
})



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
	if (!(condition))						\
		__ret = __wait_event_interruptible_lock_irq(wq,		\
						condition, lock,);	\
	__ret;								\
})

static inline int
waitqueue_active(wait_queue_head_t *q)
{
	void *c = &(q)->wchan;
	void *sq;

	sleepq_lock(c);
	sq = sleepq_lookup(c);
	sleepq_release(c);
	return (sq != NULL);
}

#define DEFINE_WAIT(name)	\
	wait_queue_t name = {}

static inline void
prepare_to_wait(wait_queue_head_t *q, wait_queue_t *wait, int state)
{
}

static inline void
finish_wait(wait_queue_head_t *q, wait_queue_t *wait)
{
}

#define wait_event_interruptible_locked(wq, condition) ({panic("implement me!!! XXX"); 0;})

#define wake_up_all_locked(x) ({panic("implement me!!! XXX"); 0;})

#endif

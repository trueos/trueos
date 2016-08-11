/*-
 * Copyright (c) 2016 Mellanox Technologies, Ltd.
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
#ifndef	_LINUX_RCUPDATE_H_
#define	_LINUX_RCUPDATE_H_

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/sx.h>

#include <linux/bug.h>
#include <linux/compiler.h>
#include <linux/ktime.h>


void call_rcu(struct rcu_head *ptr, rcu_callback_t func);
void rcu_barrier(void);
void __rcu_read_lock(void);
void __rcu_read_unlock(void);
void synchronize_rcu(void);


static inline void
kfree_call_rcu(struct rcu_head *head, rcu_callback_t func)
{
	call_rcu(head, func);
}

static inline void
rcu_read_lock(void)
{
	__rcu_read_lock();
}

static inline void
rcu_read_unlock(void)
{
	__rcu_read_unlock();
}

#define __kfree_rcu(head, offset)		\
	do { \
		kfree_call_rcu(head, (rcu_callback_t)(unsigned long)(offset)); \
	} while (0)

#define kfree_rcu(ptr, rcu_head)					\
	__kfree_rcu(&((ptr)->rcu_head), offsetof(typeof(*(ptr)), rcu_head))

#define RCU_INIT_POINTER(p, v) p=(v)

#define __rcu_access_pointer(p) \
({ \
	((typeof(*p) __force __kernel *)(READ_ONCE(p)));	\
})

#define rcu_access_pointer __rcu_access_pointer

#define __rcu_dereference_protected(p, c, space) \
({ \
	((typeof(*p) __force __kernel *)(p)); \
})

#define rcu_dereference_protected(p, c) \
	__rcu_dereference_protected((p), (c), __rcu)


#define rcu_dereference(p) rcu_dereference_protected(p, 0)

#define RCU_INITIALIZER(v) (typeof(*(v)) __force __rcu *)(v)
#define smp_store_release(p, v) atomic_store_rel_ptr((volatile unsigned long *)(p), (unsigned long)v)
#define rcu_assign_pointer(p, v) smp_store_release(&p, RCU_INITIALIZER(v))


#endif					/* _LINUX_RCUPDATE_H_ */

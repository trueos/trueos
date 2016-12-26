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


/**                                                                                                       
 * synchronize_rcu_expedited - Brute-force RCU grace period                                               
 *                                                                                                        
 * Wait for an RCU-preempt grace period, but expedite it.  The basic                                      
 * idea is to IPI all non-idle non-nohz online CPUs.  The IPI handler                                     
 * checks whether the CPU is in an RCU-preempt critical section, and                                      
 * if so, it sets a flag that causes the outermost rcu_read_unlock()                                      
 * to report the quiescent state.  On the other hand, if the CPU is                                       
 * not in an RCU read-side critical section, the IPI handler reports                                      
 * the quiescent state immediately.                                                                       
 *                                                                                                        
 * Although this is a greate improvement over previous expedited                                          
 * implementations, it is still unfriendly to real-time workloads, so is                                  
 * thus not recommended for any sort of common-case code.  In fact, if                                    
 * you are using synchronize_rcu_expedited() in a loop, please restructure                                
 * your code to batch your updates, and then Use a single synchronize_rcu()                               
 * instead.                                                                                               
 */
static inline void
synchronize_rcu_expedited(void)
{
#ifdef __linux__
        struct rcu_state *rsp = rcu_state_p;

        _synchronize_rcu_expedited(rsp, sync_rcu_exp_handler);
#else
	synchronize_rcu();
#endif
}


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


/**                                                                                                       
 * rcu_pointer_handoff() - Hand off a pointer from RCU to other mechanism                                 
 * @p: The pointer to hand off                                                                            
 *                                                                                                        
 * This is simply an identity function, but it documents where a pointer                                  
 * is handed off from RCU to some other synchronization mechanism, for                                    
 * example, reference counting or locking.  In C11, it would map to                                       
 * kill_dependency().  It could be used as follows:                                                       
 *                                                                                                        
 *      rcu_read_lock();                                                                                  
 *      p = rcu_dereference(gp);                                                                          
 *      long_lived = is_long_lived(p);                                                                    
 *      if (long_lived) {                                                                                 
 *              if (!atomic_inc_not_zero(p->refcnt))                                                      
 *                      long_lived = false;                                                               
 *              else                                                                                      
 *                      p = rcu_pointer_handoff(p);                                                       
 *      }                                                                                                 
 *      rcu_read_unlock();                                                                                
 */
#define rcu_pointer_handoff(p) (p)

#define RCU_INITIALIZER(v) (typeof(*(v)) __force __rcu *)(v)
#define smp_store_release(p, v) atomic_store_rel_ptr((volatile unsigned long *)(p), (unsigned long)v)
#define rcu_assign_pointer(p, v) smp_store_release(&p, RCU_INITIALIZER(v))


#endif					/* _LINUX_RCUPDATE_H_ */

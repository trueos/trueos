#ifndef __LINUX_FENCE_H
#define __LINUX_FENCE_H

#include <linux/err.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/bitops.h>
#include <linux/kref.h>
#include <linux/sched.h>
#include <linux/printk.h>
#include <linux/rcupdate.h>
#include <linux/kernel.h>



struct fence {
	struct kref refcount;
	const struct fence_ops *ops;
	struct rcu_head rcu;
	struct list_head cb_list;
	spinlock_t *lock;
	unsigned context, seqno;
	unsigned long flags;
	ktime_t timestamp;
	int status;
	struct list_head child_list;
	struct list_head active_list;
};

void fence_release(struct kref *kref);
void fence_free(struct fence *fence);



/**
 * fence_put - decreases refcount of the fence
 * @fence:	[in]	fence to reduce refcount of
 */
static inline void fence_put(struct fence *fence)
{
	if (fence)
		kref_put(&fence->refcount, fence_release);
}


signed long fence_wait_timeout(struct fence *, bool intr, signed long timeout);
signed long fence_wait_any_timeout(struct fence **fences, uint32_t count,
				   bool intr, signed long timeout);

/**
 * fence_wait - sleep until the fence gets signaled
 * @fence:	[in]	the fence to wait on
 * @intr:	[in]	if true, do an interruptible wait
 *
 * This function will return -ERESTARTSYS if interrupted by a signal,
 * or 0 if the fence was signaled. Other error values may be
 * returned on custom implementations.
 *
 * Performs a synchronous wait on this fence. It is assumed the caller
 * directly or indirectly holds a reference to the fence, otherwise the
 * fence might be freed before return, resulting in undefined behavior.
 */
static inline signed long fence_wait(struct fence *fence, bool intr)
{
	signed long ret;

	/* Since fence_wait_timeout cannot timeout with
	 * MAX_SCHEDULE_TIMEOUT, only valid return values are
	 * -ERESTARTSYS and MAX_SCHEDULE_TIMEOUT.
	 */
	ret = fence_wait_timeout(fence, intr, MAX_SCHEDULE_TIMEOUT);

	return ret < 0 ? ret : 0;
}

#endif

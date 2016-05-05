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

#include <linux/compat.h>

struct fence;
struct fence_cb;

typedef void (*fence_func_t)(struct fence *fence, struct fence_cb *cb);

struct fence_cb {
	struct list_head node;
	fence_func_t func;
};

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

enum fence_flag_bits {
	FENCE_FLAG_SIGNALED_BIT,
	FENCE_FLAG_ENABLE_SIGNAL_BIT,
	FENCE_FLAG_USER_BITS, /* must always be last member */
};

struct fence_ops {
	const char * (*get_driver_name)(struct fence *fence);
	const char * (*get_timeline_name)(struct fence *fence);
	bool (*enable_signaling)(struct fence *fence);
	bool (*signaled)(struct fence *fence);
	signed long (*wait)(struct fence *fence, bool intr, signed long timeout);
	void (*release)(struct fence *fence);

	int (*fill_driver_data)(struct fence *fence, void *data, int size);
	void (*fence_value_str)(struct fence *fence, char *str, int size);
	void (*timeline_value_str)(struct fence *fence, char *str, int size);
};

#define fence_free(f) kfree(f)


static inline struct fence *
fence_get(struct fence *fence)
{
	if (fence)
		kref_get(&fence->refcount);
	return (fence);
}

#define fence_get_rcu fence_get

static inline void
fence_release(struct kref *kref)
{
	struct fence *fence = container_of(kref, struct fence, refcount);

	BUG_ON(!list_empty(&fence->cb_list));
	if (fence->ops->release)
		fence->ops->release(fence);
	else
		kfree(fence);
}

static inline int
fence_signal_locked(struct fence *fence)
{
	struct fence_cb *cur, *tmp;
	int ret = 0;

	if (WARN_ON(!fence))
		return (-EINVAL);

	if (!ktime_to_ns(fence->timestamp)) {
		fence->timestamp = ktime_get();
		smp_mb__before_atomic();
	}

	if (test_and_set_bit(FENCE_FLAG_SIGNALED_BIT, &fence->flags)) {
		ret = -EINVAL;
	}
	list_for_each_entry_safe(cur, tmp, &fence->cb_list, node) {
		list_del_init(&cur->node);
		cur->func(fence, cur);
	}
	return (ret);
}

static inline void
fence_enable_sw_signaling(struct fence *fence)
{
	unsigned long flags;

	if (!test_and_set_bit(FENCE_FLAG_ENABLE_SIGNAL_BIT, &fence->flags) &&
	    !test_bit(FENCE_FLAG_SIGNALED_BIT, &fence->flags)) {
		spin_lock_irqsave(fence->lock, flags);

		if (!fence->ops->enable_signaling(fence))
			fence_signal_locked(fence);

		spin_unlock_irqrestore(fence->lock, flags);
	}
}


static inline int
fence_signal(struct fence *fence)
{
	unsigned long flags;

	if (!fence)
		return (-EINVAL);

	if (!ktime_to_ns(fence->timestamp)) {
		fence->timestamp = ktime_get();
		smp_mb__before_atomic();
	}

	if (test_and_set_bit(FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return (-EINVAL);


	if (test_bit(FENCE_FLAG_ENABLE_SIGNAL_BIT, &fence->flags)) {
		struct fence_cb *cur, *tmp;

		spin_lock_irqsave(fence->lock, flags);
		list_for_each_entry_safe(cur, tmp, &fence->cb_list, node) {
			list_del_init(&cur->node);
			cur->func(fence, cur);
		}
		spin_unlock_irqrestore(fence->lock, flags);
	}
	return (0);
}

static inline bool
fence_is_signaled(struct fence *fence)
{
	if (test_bit(FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return (true);

	if (fence->ops->signaled && fence->ops->signaled(fence)) {
		fence_signal(fence);
		return (true);
	}

	return (false);
}

static inline int64_t
fence_wait_timeout(struct fence *fence, bool intr, int64_t timeout)
{

	if (WARN_ON(timeout < 0))
		return (-EINVAL);

	if (timeout == 0)
		return (fence_is_signaled(fence));
	else
		return (fence->ops->wait(fence, intr, timeout));
}


static inline signed long
fence_wait(struct fence *fence, bool intr)
{
	signed long ret;

	ret = fence_wait_timeout(fence, intr, MAX_SCHEDULE_TIMEOUT);

	return (ret < 0 ? ret : 0);
}


static inline void
fence_put(struct fence *fence)
{
	if (fence)
		kref_put(&fence->refcount, fence_release);
}


static inline signed long
fence_wait_any_timeout(struct fence **fences, uint32_t count,
                                   bool intr, signed long timeout){
	panic("not implemented yet");
	return (0);
}

static inline unsigned
fence_context_alloc(unsigned num){
	panic("not implemented yet");
	return (0);
}

static inline int
fence_add_callback(struct fence *fence, struct fence_cb *cb,
                       fence_func_t func){
	UNIMPLEMENTED();
	return (0);
}

static inline void
fence_init(struct fence *fence, const struct fence_ops *ops,
                spinlock_t *lock, unsigned context, unsigned seqno){
	UNIMPLEMENTED();
}

static inline signed long
fence_default_wait(struct fence *fence, bool intr, signed long timeout){
	UNIMPLEMENTED();
	return (0);
}

#endif

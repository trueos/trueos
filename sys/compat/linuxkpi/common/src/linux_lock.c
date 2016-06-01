
#include <linux/ww_mutex.h>

static inline int
lock_check_stamp(struct mutex *lock, struct ww_acquire_ctx *ctx)
{
	struct ww_mutex *ww = container_of(lock, struct ww_mutex, base);
	struct ww_acquire_ctx *hold_ctx = READ_ONCE(ww->ctx);

	if (hold_ctx == NULL)
		return (0);

	if (__predict_false(ctx == hold_ctx))
		return (-EALREADY);

	if (ctx->stamp - hold_ctx->stamp <= LONG_MAX &&
	    (ctx->stamp != hold_ctx->stamp || ctx > hold_ctx)) {
		return (-EDEADLK);
	}

	return (0);
}

int
linux_mutex_lock_common(struct mutex *lock, int state, struct ww_acquire_ctx *ctx)
{
	struct sx *sx;
	struct ww_mutex *ww;
	int rc;

	sx = &lock->sx;
	if (SCHEDULER_STOPPED())
		return (0);

	if (ctx && ctx->acquired > 0) {
		if ((rc = lock_check_stamp(lock, ctx)))
			return (rc);
	}
	if (state == TASK_UNINTERRUPTIBLE)
		mutex_lock(lock);
	else if (state == TASK_INTERRUPTIBLE)
		mutex_lock_interruptible(lock);
	else
		panic("unknown state %d", state);

	if (ctx) {
		ww = container_of(lock, struct ww_mutex, base);
		ctx->acquired++;
		ww->ctx = ctx;
	}

	return (0);
}



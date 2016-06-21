
#include <linux/ww_mutex.h>
#include <linux/wait.h>
#include <linux/spinlock.h>

int		db_printf(const char *fmt, ...) __printflike(1, 2);

#define	mtxlock2mtx(c)	(__containerof(c, struct mtx, mtx_lock))
#define	mtx_owner(m)	((struct thread *)((m)->mtx_lock & ~MTX_FLAGMASK))
#define mtx_unowned(m)	((m)->mtx_lock == MTX_UNOWNED)
#define	mtx_destroyed(m) ((m)->mtx_lock == MTX_DESTROYED)

void
linux_mtx_sysinit(void *arg)
{
	struct mtx_args *margs = arg;

	linux_mtx_init((struct mtx *)margs->ma_mtx, margs->ma_desc, NULL,
	    margs->ma_opts);
}

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
_linux_mutex_lock_common(struct mutex *lock, int state, struct ww_acquire_ctx *ctx, char *file, int line)
{
	struct sx *sx;
	struct ww_mutex *ww;
	int rc;
	sx = &lock->sx;
	if (SKIP_SLEEP())
		return (0);

	if (ctx && ctx->acquired > 0) {
		if ((rc = lock_check_stamp(lock, ctx)))
			return (rc);
	}
	if (state == TASK_UNINTERRUPTIBLE)
		_sx_xlock(&lock->sx, 0, file, line);
	else if (state == TASK_INTERRUPTIBLE)
		_sx_xlock(&lock->sx, SX_INTERRUPTIBLE, file, line);
	else
		panic("unknown state %d", state);

	if (ctx) {
		ww = container_of(lock, struct ww_mutex, base);
		ctx->acquired++;
		ww->ctx = ctx;
	}

	return (0);
}

void
_linux_mtx_init(volatile uintptr_t *c, const char *name, const char *type, int opts)
{
	struct mtx *m;
	struct lock_class *class;
	int flags;

	m = mtxlock2mtx(c);

	MPASS((opts & ~(MTX_QUIET | MTX_RECURSE |
	    MTX_NOWITNESS | MTX_DUPOK | MTX_NOPROFILE | MTX_NEW | MTX_INTEROP)) == 0);
	ASSERT_ATOMIC_LOAD_PTR(m->mtx_lock,
	    ("%s: mtx_lock not aligned for %s: %p", __func__, name,
	    &m->mtx_lock));

	/* Determine lock class and lock flags. */

	MPASS(opts & MTX_INTEROP);
	class = &lock_class_mtx_interop;

	flags = 0;
	if (opts & MTX_QUIET)
		flags |= LO_QUIET;
	if (opts & MTX_RECURSE)
		flags |= LO_RECURSABLE;
	if ((opts & MTX_NOWITNESS) == 0)
		flags |= LO_WITNESS;
	if (opts & MTX_DUPOK)
		flags |= LO_DUPOK;
	if (opts & MTX_NOPROFILE)
		flags |= LO_NOPROFILE;
	if (opts & MTX_NEW)
		flags |= LO_NEW;

	/* Initialize mutex. */
	lock_init(&m->lock_object, class, name, type, flags);

	m->mtx_lock = MTX_UNOWNED;
	m->mtx_recurse = 0;
}

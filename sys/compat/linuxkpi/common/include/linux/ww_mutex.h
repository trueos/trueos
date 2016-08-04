#ifndef	_LINUX_WW_MUTEX_H_
#define	_LINUX_WW_MUTEX_H_

#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/atomic.h>

struct ww_class {
	atomic_long_t stamp;
#ifdef notyet	
	struct lock_class_key acquire_key;
	struct lock_class_key mutex_key;
#endif	
	const char *acquire_name;
	const char *mutex_name;
};

struct ww_acquire_ctx {
	struct task_struct *task;
	unsigned long stamp;
	unsigned acquired;
};
struct ww_mutex {
	struct mutex base;
	struct ww_acquire_ctx *ctx;
};

#ifdef CONFIG_DEBUG_LOCK_ALLOC
# define __WW_CLASS_MUTEX_INITIALIZER(lockname, ww_class) \
		, .ww_class = &ww_class
#else
# define __WW_CLASS_MUTEX_INITIALIZER(lockname, ww_class)
#endif

#define __WW_CLASS_INITIALIZER(ww_class) \
		{ .stamp = ATOMIC_LONG_INIT(0) \
		, .acquire_name = #ww_class "_acquire" \
		, .mutex_name = #ww_class "_mutex" }

#define __WW_MUTEX_INITIALIZER(lockname, class) \
		{ .base = { \__MUTEX_INITIALIZER(lockname) } \
		__WW_CLASS_MUTEX_INITIALIZER(lockname, class) }

#define DEFINE_WW_CLASS(classname) \
	struct ww_class classname = __WW_CLASS_INITIALIZER(classname)

#define DEFINE_WW_MUTEX(mutexname, ww_class) \
	struct ww_mutex mutexname = __WW_MUTEX_INITIALIZER(mutexname, ww_class)


#define	ww_mutex_is_locked(_m)		sx_xlocked(&(_m)->base.sx)
#define ww_mutex_lock_slow(m, x)  ww_mutex_lock(m, x)

static inline int __must_check
ww_mutex_trylock(struct ww_mutex *lock)
{
	return mutex_trylock(&lock->base);
}

static inline int
ww_mutex_lock(struct ww_mutex *lock, struct ww_acquire_ctx *ctx)
{
	if (mutex_is_owned(&lock->base))
		return (-EALREADY);
	if (ctx)
		return (linux_mutex_lock_common(&lock->base, TASK_UNINTERRUPTIBLE, ctx));

	mutex_lock(&lock->base);
	return (0);
}

static inline int
ww_mutex_lock_interruptible(struct ww_mutex *lock, struct ww_acquire_ctx *ctx)
{
	if (mutex_is_locked(&lock->base))
		return (-EALREADY);
	if (ctx)
		return (linux_mutex_lock_common(&lock->base, TASK_INTERRUPTIBLE, ctx));

	mutex_lock_interruptible(&lock->base);
	return (0);
}

static inline void
ww_mutex_unlock(struct ww_mutex *lock)
{

	if (lock->ctx) {
		if (lock->ctx->acquired > 0)
			lock->ctx->acquired--;
		lock->ctx = NULL;
	}
	mutex_unlock(&lock->base);
}

static inline void
ww_mutex_destroy(struct ww_mutex *lock)
{
	mutex_destroy(&lock->base);
}

#define ww_mutex_lock_slow_interruptible(m, x) ww_mutex_lock_interruptible(m, x)

static inline void
ww_acquire_init(struct ww_acquire_ctx *ctx, struct ww_class *ww_class)
{
	ctx->task = current;
	ctx->stamp = atomic_long_inc_return(&ww_class->stamp);
	ctx->acquired = 0;
}


#define ww_mutex_init(lock, class) _ww_mutex_init((lock), (class), __FILE__, __LINE__)

static inline void
_ww_mutex_init(struct ww_mutex *lock, struct ww_class *ww_class, char *file, int line)
{
#ifdef WITNESS_ALL
	linux_mutex_init(&lock->base, ww_class->mutex_name, SX_DUPOK, file, line);
#else
	linux_mutex_init(&lock->base, ww_class->mutex_name, SX_NOWITNESS, NULL, 0);
#endif	
	
}

static inline void
ww_acquire_fini(struct ww_acquire_ctx *ctx) { }

static inline void
ww_acquire_done(struct ww_acquire_ctx *ctx) { }

#endif

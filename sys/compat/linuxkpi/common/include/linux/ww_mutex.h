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
#ifdef __linux__
struct ww_mutex {
	struct sx base;
	struct ww_acquire_ctx *ctx;
};
#endif
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


#define ww_mutex mutex
#define ww_mutex_destroy(m) linux_mutex_destroy(m)

#define	ww_mutex_is_locked(_m)		sx_xlocked(&(_m)->sx)
#define ww_mutex_lock_slow(m, x)  ww_mutex_lock(m, x)
#define ww_mutex_trylock mutex_trylock
#define ww_mutex_lock_interruptible(m, x) mutex_lock_interruptible(m)
#define ww_mutex_lock_slow_interruptible(m, x) mutex_lock_interruptible(m)


#define ww_mutex_unlock(m)			\
	do {					\
		mutex_unlock(m);		\
	} while (0)


static inline int
ww_mutex_lock(struct ww_mutex *m, struct ww_acquire_ctx *ctx)
{
	if (sx_xlocked(&m->sx))
		return (-EALREADY);
	sx_xlock(&m->sx);
	return (0);
}

/*
 * XXX FIX ME
 */


static inline void
ww_acquire_init(struct ww_acquire_ctx *ctx, struct ww_class *ww_class)
{
}

static inline void
ww_acquire_fini(struct ww_acquire_ctx *ctx) { }

static inline void
__ww_mutex_init(struct ww_mutex *lock, struct ww_class *ww_class, char *name)
{
	linux_mutex_init(lock, name, SX_DUPOK);
}

#define ww_mutex_init(l, w) __ww_mutex_init(l, w, #l)

static inline void
ww_acquire_done(struct ww_acquire_ctx *ctx) { }

#endif

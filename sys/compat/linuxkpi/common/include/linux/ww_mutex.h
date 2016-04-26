#ifndef	_LINUX_WW_MUTEX_H_
#define	_LINUX_WW_MUTEX_H_

#include <linux/mutex.h>


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
#define ww_mutex_init(m, class) linux_mutex_init(m, #m)
#define ww_mutex_destroy(m) linux_mutex_destroy(m)

#define	ww_mutex_is_locked(_m)		sx_xlocked(&(_m)->sx)
#endif

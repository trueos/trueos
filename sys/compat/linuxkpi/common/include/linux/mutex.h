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
#ifndef	_LINUX_MUTEX_H_
#define	_LINUX_MUTEX_H_

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/sx.h>

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/lockdep.h>

typedef struct mutex {
	struct sx sx;
} mutex_t;

#define	MUTEX_SKIP(void) unlikely(SCHEDULER_STOPPED() || kdb_active)

#define	sx_is_owned(sx) \
	(((sx)->sx_lock & ~(SX_LOCK_FLAGMASK & ~SX_LOCK_SHARED)) == \
	(uintptr_t)curthread)

#define	sx_is_xlocked(sx) \
	(((sx)->sx_lock & ~(SX_LOCK_FLAGMASK)) != (uintptr_t)NULL)

#define	mutex_lock(_m) do {			\
	if (!MUTEX_SKIP())			\
		sx_xlock(&(_m)->sx);		\
} while (0)

#define	mutex_lock_nested(_m, _s)	mutex_lock(_m)
#define mutex_lock_nest_lock(_m, _s)	mutex_lock(_m)

#define	mutex_lock_interruptible(_m) ({		\
	MUTEX_SKIP() ? 0 :			\
	(sx_xlock_sig(&(_m)->sx) ? -EINTR : 0);	\
})

#define	mutex_unlock(_m) do {			\
	if (!MUTEX_SKIP())			\
		sx_xunlock(&(_m)->sx);		\
} while (0)

#define	mutex_trylock(_m) ({			\
	MUTEX_SKIP() ? 1 :			\
	!!sx_try_xlock(&(_m)->sx);		\
})

#define	mutex_is_locked(_m)		sx_is_xlocked(&(_m)->sx)
#define	mutex_is_owned(_m)		sx_is_owned(&(_m)->sx)

#ifdef WITNESS_ALL
/* NOTE: the maximum WITNESS name is 64 chars */
#define	__mutex_name(name, file, line)		\
	(((const char *){file ":" #line "-" name}) + 	\
	(sizeof(file) > 16 ? sizeof(file) - 16 : 0))
#else
#define	__mutex_name(name, file, line)	name
#endif
#define	_mutex_name(...)		__mutex_name(__VA_ARGS__)
#define	mutex_name(name)		_mutex_name(name, __FILE__, __LINE__)

#define DEFINE_MUTEX(lock)						\
	mutex_t lock;							\
	SX_SYSINIT_FLAGS(lock, &(lock).sx, mutex_name(#lock), SX_DUPOK)

static inline void
linux_mutex_init(mutex_t *m, const char *name, int flags)
{

	memset(m, 0, sizeof(*m));
	sx_init_flags(&m->sx, name,  flags);
}

struct ww_acquire_ctx;

#define linux_mutex_lock_common(m, state, ctx)  _linux_mutex_lock_common((m), (state), (ctx), __FILE__, __LINE__)
int _linux_mutex_lock_common(struct mutex *m, int state, struct ww_acquire_ctx *ctx, char *file, int line);

static inline void
linux_mutex_destroy(mutex_t *m)
{
	if (mutex_is_owned(m))
		mutex_unlock(m);
	sx_destroy(&m->sx);
}

#define	mutex_init(m) \
	linux_mutex_init(m, mutex_name(#m), SX_DUPOK)
#ifdef WITNESS_ALL
#define	mutex_init_nowitness(m) \
	linux_mutex_init(m, mutex_name(#m), 0)
#else
#define	mutex_init_nowitness(m) \
	linux_mutex_init(m, mutex_name(#m), SX_NOWITNESS)
#endif
#define mutex_destroy(m) \
	linux_mutex_destroy(m)

#endif	/* _LINUX_MUTEX_H_ */

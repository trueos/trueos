/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
 * Copyright (c) 2016 Matthew Macy
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
#ifndef	_LINUX_SPINLOCK_H_
#define	_LINUX_SPINLOCK_H_

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <linux/compiler.h>
#include <linux/irqflags.h>
#include <linux/kernel.h>
#include <linux/rwlock.h>
#include <linux/lkpi_mutex.h>

typedef struct {
	struct mtx m;
} spinlock_t;


#define	spin_lock(_l)		lkpi_mtx_lock(&(_l)->m)
#define	spin_unlock(_l)		lkpi_mtx_unlock(&(_l)->m)
#define	spin_trylock(_l)	mtx_trylock(&(_l)->m)
#define	spin_lock_nested(_l, _n) mtx_lock_flags(&(_l)->m, MTX_DUPOK)

void	_linux_mtx_init(volatile uintptr_t *c, const char *name, const char *type,
	    int opts);

#define spin_lock_init(lock) _spin_lock_init((lock), #lock, __FILE__, __LINE__)

static inline void
_spin_lock_init(spinlock_t *lock, char *name, char *file, int line)
{
#ifdef WITNESS_ALL
	char buf[64];
#endif

	memset(&lock->m, 0, sizeof(lock->m));
#ifdef WITNESS_ALL
	snprintf(buf, 64, "%s:%s:%d", name, file, line);
	lkpi_mtx_init(&lock->m, strdup(buf, M_DEVBUF), NULL, 0);
#else
	lkpi_mtx_init(&lock->m, name, NULL, MTX_NOWITNESS);
#endif	
}

static inline void
spin_lock_destroy(spinlock_t *lock)
{
	mtx_destroy(&lock->m);
}

void	linux_mtx_sysinit(void *arg);

#define	LINUX_MTX_SYSINIT(name, mtx, desc, opts)				\
	static struct mtx_args name##_args = {				\
		(mtx),							\
		(desc),							\
		(opts)							\
	};								\
	SYSINIT(name##_mtx_sysinit, SI_SUB_LOCK, SI_ORDER_MIDDLE,	\
	    linux_mtx_sysinit, &name##_args);					\
	SYSUNINIT(name##_mtx_sysuninit, SI_SUB_LOCK, SI_ORDER_MIDDLE,	\
	    _mtx_destroy, __DEVOLATILE(void *, &(mtx)->mtx_lock))

#define	DEFINE_SPINLOCK(lock)						\
	spinlock_t lock;						\
	LINUX_MTX_SYSINIT(lock, &(lock).m, #lock, 0)


static inline void
assert_spin_locked(spinlock_t *lock)
{
	mtx_assert(&lock->m, MA_OWNED);
}

#define spin_lock_bh(lock) _spin_lock_bh((lock), __FILE__, __LINE__)

static inline void _spin_lock_bh(spinlock_t *lock, char *file, int line) {
	critical_enter();
	_mtx_lock_flags(&lock->m, 0, file, line);
}
static inline void spin_unlock_bh(spinlock_t *lock) {
	spin_unlock(lock);
	critical_exit();
}


#define	spin_lock_irq(_l)	lkpi_mtx_lock_spin(&(_l)->m)
#define	spin_unlock_irq(_l)	lkpi_mtx_unlock_spin(&(_l)->m)

#define spin_lock_irqsave(lock, flags) do {	\
		flags = local_save_flags();	\
		spin_lock_irq((lock));		\
	} while (0)

/* is the local_irq_restore really necessary since we track interrupt nesting? */
#define	spin_unlock_irqrestore(lock, flags) do {	\
		spin_unlock_irq((lock));		\
		flags = 0;				\
	} while (0)


#endif	/* _LINUX_SPINLOCK_H_ */

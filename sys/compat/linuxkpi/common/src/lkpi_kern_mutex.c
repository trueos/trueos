/*-
 * Copyright (c) 1998 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from BSDI $Id: mutex_witness.c,v 1.1.2.20 2000/04/27 03:10:27 cp Exp $
 *	and BSDI $Id: synch_machdep.c,v 2.3.2.39 2000/04/27 03:10:25 cp Exp $
 */

/*
 * Machine independent bits of mutex implementation.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_sched.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/turnstile.h>
#include <sys/vmmeter.h>
#include <sys/lock_profile.h>

#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/cpu.h>

#include <ddb/ddb.h>

#include <fs/devfs/devfs_int.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <linux/lkpi_mutex.h>

#if defined(SMP)
#define	ADAPTIVE_MUTEXES
#endif

static void lkpi_interop_init(void *arg __unused);
SYSINIT(lkpi_kern_mutex_init, SI_SUB_DRIVERS, SI_ORDER_FIRST, lkpi_interop_init, NULL);

/*
 * Return the mutex address when the lock cookie address is provided.
 * This functionality assumes that struct mtx* have a member named mtx_lock.
 */
#define	mtxlock2mtx(c)	(__containerof(c, struct mtx, mtx_lock))

/*
 * Internal utility macros.
 */
#define mtx_unowned(m)	((m)->mtx_lock == MTX_UNOWNED)

#define	mtx_destroyed(m) ((m)->mtx_lock == MTX_DESTROYED)

#define	mtx_owner(m)	((struct thread *)((m)->mtx_lock & ~MTX_FLAGMASK))



static void
lkpi_lock_spin(struct lock_object *lock, uintptr_t how)
{

	lkpi_mtx_lock_spin((struct mtx*)lock);
}

static uintptr_t
lkpi_unlock_spin(struct lock_object *lock)
{
	struct mtx *m;

	m = (struct mtx *)lock;
	mtx_assert(m, MA_OWNED | MA_NOTRECURSED);
	lkpi_mtx_unlock_spin(m);
	return (0);
}

static void
lkpi_interop_init(void *arg __unused)
{
	lock_class_mtx_interop.lc_lock = lkpi_lock_spin;
	lock_class_mtx_interop.lc_unlock = lkpi_unlock_spin;

}

/*
 * Function versions of the inlined __mtx_* macros.  These are used by
 * modules and can also be called from assembly language if needed.
 */
void
__lkpi_mtx_lock_flags(volatile uintptr_t *c, int opts, const char *file, int line)
{
	struct thread *td = curthread;
	struct mtx *m;
	int intrctx;

	if (SCHEDULER_STOPPED())
		return;

	intrctx = (td->td_critnest || td->td_intr_nesting_level);
	m = mtxlock2mtx(c);

	KASSERT(m->mtx_lock != MTX_DESTROYED,
	    ("mtx_lock() of destroyed mutex @ %s:%d", file, line));

	if (!intrctx)
		WITNESS_CHECKORDER(&m->lock_object, (opts & ~MTX_RECURSE) |
				   LOP_NEWORDER | LOP_EXCLUSIVE, file, line, NULL);
	
	__mtx_lock(m, curthread, opts, file, line);
	if (!intrctx) {
		LOCK_LOG_LOCK("LOCK", &m->lock_object, opts, m->mtx_recurse, file,
		      line);
		WITNESS_LOCK(&m->lock_object, (opts & ~MTX_RECURSE) | LOP_EXCLUSIVE,
			     file, line);
		TD_LOCKS_INC(curthread);
	}
}

void
__lkpi_mtx_unlock_flags(volatile uintptr_t *c, int opts, const char *file, int line)
{
	struct thread *td = curthread;
	struct mtx *m;
	int nonintrctx;

	if (SCHEDULER_STOPPED())
		return;

	m = mtxlock2mtx(c);
	nonintrctx = !(td->td_critnest || td->td_intr_nesting_level);
	
	KASSERT(m->mtx_lock != MTX_DESTROYED,
	    ("mtx_unlock() of destroyed mutex @ %s:%d", file, line));
	if (nonintrctx) { 
		WITNESS_UNLOCK(&m->lock_object, opts | LOP_EXCLUSIVE, file, line);
		LOCK_LOG_LOCK("UNLOCK", &m->lock_object, opts, m->mtx_recurse, file,
			      line);
	}
	mtx_assert(m, MA_OWNED);

	__mtx_unlock(m, curthread, opts, file, line);
	if (nonintrctx)
		TD_LOCKS_DEC(curthread);
}

void
__lkpi_mtx_lock_spin_flags(volatile uintptr_t *c, int opts, const char *file,
    int line)
{
	struct mtx *m;

	if (SCHEDULER_STOPPED())
		return;

	m = mtxlock2mtx(c);

	KASSERT(m->mtx_lock != MTX_DESTROYED,
	    ("lkpi_mtx_lock_spin() of destroyed mutex @ %s:%d", file, line));
	if (mtx_owned(m))
		KASSERT((m->lock_object.lo_flags & LO_RECURSABLE) != 0 ||
		    (opts & MTX_RECURSE) != 0,
	    ("lkpi_mtx_lock_spin: recursed on non-recursive mutex %s @ %s:%d\n",
		    m->lock_object.lo_name, file, line));
	opts &= ~MTX_RECURSE;
	__mtx_lock_spin(m, curthread, opts, file, line);
}

void
__lkpi_mtx_unlock_spin_flags(volatile uintptr_t *c, int opts, const char *file,
    int line)
{
	struct mtx *m;

	if (SCHEDULER_STOPPED())
		return;

	m = mtxlock2mtx(c);

	KASSERT(m->mtx_lock != MTX_DESTROYED,
	    ("lkpi_mtx_unlock_spin() of destroyed mutex @ %s:%d", file, line));
	mtx_assert(m, MA_OWNED);
	__mtx_unlock_spin(m);
}

/*
 * The important part of mtx_trylock{,_flags}()
 * Tries to acquire lock `m.'  If this function is called on a mutex that
 * is already owned, it will recursively acquire the lock.
 */
int
__lkpi_mtx_trylock_flags(volatile uintptr_t *c, int opts, const char *file, int line)
{
	struct mtx *m;
#ifdef LOCK_PROFILING
	uint64_t waittime = 0;
	int contested = 0;
#endif
	int rval;

	if (SCHEDULER_STOPPED())
		return (1);

	m = mtxlock2mtx(c);

	KASSERT(kdb_active != 0 || !TD_IS_IDLETHREAD(curthread),
	    ("mtx_trylock() by idle thread %p on sleep mutex %s @ %s:%d",
	    curthread, m->lock_object.lo_name, file, line));
	KASSERT(m->mtx_lock != MTX_DESTROYED,
	    ("mtx_trylock() of destroyed mutex @ %s:%d", file, line));

	if (mtx_owned(m) && ((m->lock_object.lo_flags & LO_RECURSABLE) != 0 ||
	    (opts & MTX_RECURSE) != 0)) {
		m->mtx_recurse++;
		atomic_set_ptr(&m->mtx_lock, MTX_RECURSED);
		rval = 1;
	} else
		rval = _mtx_obtain_lock(m, (uintptr_t)curthread);
	opts &= ~MTX_RECURSE;

	LOCK_LOG_TRY("LOCK", &m->lock_object, opts, rval, file, line);
	if (rval) {
		WITNESS_LOCK(&m->lock_object, opts | LOP_EXCLUSIVE | LOP_TRYLOCK,
		    file, line);
		TD_LOCKS_INC(curthread);
		if (m->mtx_recurse == 0)
			LOCKSTAT_PROFILE_OBTAIN_LOCK_SUCCESS(adaptive__acquire,
			    m, contested, waittime, file, line);

	}

	return (rval);
}

/*
 * Mutex initialization routine; initialize lock `m' of type contained in
 * `opts' with options contained in `opts' and name `name.'  The optional
 * lock type `type' is used as a general lock category name for use with
 * witness.
 */
void
__lkpi_mtx_init(volatile uintptr_t *c, const char *name, const char *type, int opts)
{
	struct mtx *m;
	struct lock_class *class;
	int flags;

	m = mtxlock2mtx(c);

	MPASS((opts & ~(MTX_SPIN | MTX_QUIET | MTX_RECURSE |
	    MTX_NOWITNESS | MTX_DUPOK | MTX_NOPROFILE | MTX_NEW)) == 0);
	ASSERT_ATOMIC_LOAD_PTR(m->mtx_lock,
	    ("%s: mtx_lock not aligned for %s: %p", __func__, name,
	    &m->mtx_lock));

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

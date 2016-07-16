
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ktrace.h"
#include "opt_sched.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/condvar.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/sdt.h>
#include <sys/signalvar.h>
#include <sys/sleepqueue.h>
#include <sys/smp.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/vmmeter.h>
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif

#include <machine/cpu.h>

#include <linux/sched.h>
#include <linux/wait.h>

static int
lkpi_msleep_spin_sbt(void *ident, struct mtx *mtx, int prio, const char *wmesg,
    sbintime_t sbt, sbintime_t pr, int flags)
{
	struct thread *td;
	struct proc *p;
	int rval;
	int catch, pri;
	WITNESS_SAVE_DECL(mtx);

	td = curthread;
	p = td->td_proc;
	KASSERT(mtx != NULL, ("sleeping without a mutex"));
	KASSERT(p != NULL, ("msleep1"));
	KASSERT(ident != NULL && TD_IS_RUNNING(td), ("msleep"));

	if (SCHEDULER_STOPPED())
		return (0);

	catch = prio & PCATCH;
	pri = prio & PRIMASK;
	sleepq_lock(ident);
	CTR5(KTR_PROC, "msleep_spin: thread %ld (pid %ld, %s) on %s (%p)",
	    td->td_tid, p->p_pid, td->td_name, wmesg, ident);

	DROP_GIANT();
	mtx_assert(mtx, MA_OWNED | MA_NOTRECURSED);
	WITNESS_SAVE(&mtx->lock_object, mtx);
	lkpi_mtx_unlock_spin(mtx);

	/*
	 * We put ourselves on the sleep queue and start our timeout.
	 */
	sleepq_add(ident, &mtx->lock_object, wmesg, SLEEPQ_SLEEP, 0);
	if (sbt != 0)
		sleepq_set_timeout_sbt(ident, sbt, pr, flags);

	/*
	 * Can't call ktrace with any spin locks held so it can lock the
	 * ktrace_mtx lock, and WITNESS_WARN considers it an error to hold
	 * any spin lock.  Thus, we have to drop the sleepq spin lock while
	 * we handle those requests.  This is safe since we have placed our
	 * thread on the sleep queue already.
	 */
#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW)) {
		sleepq_release(ident);
		ktrcsw(1, 0, wmesg);
		sleepq_lock(ident);
	}
#endif
	if (sbt != 0 && catch)
		rval = sleepq_timedwait_sig(ident, pri);
	else if (sbt != 0)
		rval = sleepq_timedwait(ident, pri);
	else if (catch)
		rval = sleepq_wait_sig(ident, pri);
	else {
		sleepq_wait(ident, pri);
		rval = 0;
	}

#ifdef KTRACE
	if (KTRPOINT(td, KTR_CSW))
		ktrcsw(0, 0, wmesg);
#endif
	PICKUP_GIANT();
	if (!(prio & PDROP))
		lkpi_mtx_lock_spin(mtx);
	return (rval);
}


long
schedule_timeout(signed long timeout)
{
	int ret, flags, sleepable;
	struct mtx *m;
	struct mtx stackm;

	if (timeout < 0)
		return 0;
	if (SKIP_SLEEP())
		return (0);
	MPASS(current);
	sleepable = 0;
	if (current->sleep_wq == NULL) {
		m = &stackm;
		bzero(m, sizeof(*m));
		mtx_init(m, "stack", NULL, MTX_DEF|MTX_NOWITNESS);
		mtx_lock(m);
		sleepable = 1;
	} else {
		m = &current->sleep_wq->lock.m;
		lkpi_mtx_lock_spin(m);
		if (current->state == TASK_WAKING) {
			lkpi_mtx_unlock_spin(m);
			set_current_state(TASK_RUNNING);
			return (0);
		}
	}

	flags = (current->state == TASK_INTERRUPTIBLE) ? PCATCH : 0;
	if (sleepable)
		ret = _sleep(current, &(m->lock_object), flags | PDROP ,
			     "lsti", tick_sbt * timeout, 0 , C_HARDCLOCK);
	else 
		ret = lkpi_msleep_spin_sbt(current, m, flags | PDROP, "lstisp",
				      tick_sbt * timeout, 0, C_HARDCLOCK);

	set_current_state(TASK_RUNNING);

	return (-ret);
}

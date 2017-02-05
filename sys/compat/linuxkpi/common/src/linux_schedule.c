
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
#include <linux/fs.h>
#include <linux/list.h>

long
schedule_timeout(signed long timeout)
{
	return (schedule_timeout_locked(timeout, NULL));
}

long
schedule_timeout_locked(signed long timeout, spinlock_t *lock)
{
	int flags, expire;
	long ret;
	struct mtx *m;
	struct mtx stackm;

	if (timeout < 0)
		return 0;
	expire = ticks + (unsigned int)timeout;
	if (SKIP_SLEEP())
		return (0);
	MPASS(current);
	if (current->state == TASK_WAKING)
		goto done;

	if (lock != NULL) {
		m = &lock->m;
	} else if (current->sleep_wq != NULL) {
		m = &current->sleep_wq->lock.m;
		mtx_lock(m);
	} else {
		m = &stackm;
		bzero(m, sizeof(*m));
		mtx_init(m, "stack", NULL, MTX_DEF | MTX_NOWITNESS);
		mtx_lock(m);
	}

	flags = (current->state == TASK_INTERRUPTIBLE) ? PCATCH : 0;
	if (lock == NULL)
		flags |= PDROP;
	ret = _sleep(current, &(m->lock_object), flags,
	     "lsti", tick_sbt * timeout, 0 , C_HARDCLOCK);

done:
	set_current_state(TASK_RUNNING);
	if (timeout == MAX_SCHEDULE_TIMEOUT)
		ret = MAX_SCHEDULE_TIMEOUT;
	else
		ret = expire - ticks;

	return (ret);
}

void
__wake_up(wait_queue_head_t *q, int mode, int nr, void *key)
{
	int flags;
	struct list_head *p, *ptmp;
	struct linux_file *f;

	spin_lock_irqsave(&q->lock, flags);
	selwakeup(&q->wqh_si);
	if (__predict_false(!list_empty(&q->wqh_file_list))) {
		list_for_each_safe(p, ptmp, &q->wqh_file_list) {
			f = list_entry(p, struct linux_file, f_entry);
			tasklet_schedule(&f->f_kevent_tasklet);
		}
	}
	__wake_up_locked(q, mode, nr, key);
	spin_unlock_irqrestore(&q->lock, flags);
}

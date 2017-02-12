
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
schedule_timeout(long timeout)
{
	struct mtx stackm;
	struct mtx *m;
	sbintime_t sbt;
	long ret = 0;
	int delta;
	int flags;

	/* under FreeBSD jiffies are 32-bit */
	timeout = (int)timeout;

	/* check for invalid timeout or panic */
	if (timeout < 0 || SKIP_SLEEP())
		goto done;

	/* store current ticks value */
	delta = ticks;

	MPASS(current);

	/* check if about to wake up */
	if (current->state == TASK_WAKING)
		goto done;

	/* get mutex to use */
	if (current->sleep_wq != NULL) {
		m = &current->sleep_wq->lock.m;
	} else {
		m = &stackm;
		memset(m, 0, sizeof(*m));
		mtx_init(m, "stack", NULL, MTX_DEF | MTX_NOWITNESS);
	}
	mtx_lock(m);

	/* get sleep flags */
	flags = (current->state == TASK_INTERRUPTIBLE) ?
	    (PCATCH | PDROP) : PDROP;

	/* compute timeout value to use */
	if (timeout == MAX_SCHEDULE_TIMEOUT)
		sbt = 0;			/* infinite timeout */
	else if (timeout < 1)
		sbt = tick_sbt;			/* avoid underflow */
	else
		sbt = tick_sbt * timeout;	/* normal case */

	(void) _sleep(current, &m->lock_object, flags,
	    "lsti", sbt, 0 , C_HARDCLOCK);

	/* compute number of ticks consumed */
	delta = (ticks - delta);

	/* compute number of ticks left from timeout */
	ret = timeout - delta;

	/* check for underflow or overflow */
	if (ret < 0 || delta < 0)
		ret = 0;
done:
	set_current_state(TASK_RUNNING);
	return ((timeout == MAX_SCHEDULE_TIMEOUT) ? timeout : ret);
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

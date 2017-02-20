
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
	struct task_struct *task;
	sbintime_t sbt;
	long ret = 0;
	int state;
	int delta;

	/* under FreeBSD jiffies are 32-bit */
	timeout = (int)timeout;

	/* get task pointer */
	task = current;

	MPASS(task);

	mtx_lock(&task->sleep_lock);

	/* check for invalid timeout or panic */
	if (timeout < 0 || SKIP_SLEEP())
		goto done;

	/* store current ticks value */
	delta = ticks;

	state = atomic_read(&task->state);

	/* check if about to wake up */
	if (state != TASK_WAKING) {
		int flags;

		/* get sleep flags */
		flags = (state == TASK_INTERRUPTIBLE) ? PCATCH : 0;

		/* compute timeout value to use */
		if (timeout == MAX_SCHEDULE_TIMEOUT)
			sbt = 0;		/* infinite timeout */
		else if (timeout < 1)
			sbt = tick_sbt;		/* avoid underflow */
		else
			sbt = tick_sbt * timeout;	/* normal case */

		(void) _sleep(task, &task->sleep_lock.lock_object, flags,
		    "lsti", sbt, 0 , C_HARDCLOCK);

		/* compute number of ticks consumed */
		delta = (ticks - delta);
	} else {
		/* no ticks consumed */
		delta = 0;
	}

	/* compute number of ticks left from timeout */
	ret = timeout - delta;

	/* check for underflow or overflow */
	if (ret < 0 || delta < 0)
		ret = 0;
done:
	atomic_set(&task->state, TASK_RUNNING);

	mtx_unlock(&task->sleep_lock);

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

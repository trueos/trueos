#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/compat.h>

#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/priority.h>
#include <sys/kthread.h>
#include <sys/sched.h>

enum {
	KTHREAD_SHOULD_STOP,
	KTHREAD_SHOULD_PARK,
	KTHREAD_IS_PARKED,
};

bool
kthread_should_stop_task(struct task_struct *ts)
{
	return (test_bit(KTHREAD_SHOULD_STOP, &ts->kthread_flags));
}

bool
kthread_should_stop(void)
{
	return (test_bit(KTHREAD_SHOULD_STOP, &current->kthread_flags));
}

bool
kthread_should_park(void)
{
	return (test_bit(KTHREAD_SHOULD_PARK, &current->kthread_flags));
}

int
kthread_park(struct task_struct *ts)
{
	int ret = -ENOSYS;

/* XXX we don't know the thread is live */
	if (ts != NULL) {
		if (!test_bit(KTHREAD_IS_PARKED, &ts->kthread_flags)) {
			set_bit(KTHREAD_SHOULD_PARK, &ts->kthread_flags);
			if (ts != current) {
				wake_up_process(ts);
				wait_for_completion(&ts->parked);
			}
		}
		ret = 0;
	}
	return ret;
}

void
kthread_unpark(struct task_struct *ts)
{
	clear_bit(KTHREAD_SHOULD_PARK, &ts->kthread_flags);
	if (test_and_clear_bit(KTHREAD_IS_PARKED, &ts->kthread_flags))
		wake_up_state(ts, TASK_PARKED);
}

void
kthread_parkme(void)
{
	struct task_struct *ts = current;

	MPASS(ts != NULL);

	ts->state = TASK_PARKED;
	while (test_bit(KTHREAD_SHOULD_PARK, &ts->kthread_flags)) {
		if (!test_and_set_bit(KTHREAD_IS_PARKED, &ts->kthread_flags))
			complete(&ts->parked);
		schedule();
		ts->state = TASK_PARKED;
	}
	clear_bit(KTHREAD_IS_PARKED, &ts->kthread_flags);
	ts->state = TASK_RUNNING;
}

int
kthread_stop(struct task_struct *task)	
{
	struct thread *td;
	int retval = 0;

	/* XXX we don't know the thread is live */
	td = task->task_thread;
	PROC_LOCK(td->td_proc);
	set_bit(KTHREAD_SHOULD_STOP, &task->kthread_flags);
	PROC_UNLOCK(td->td_proc);
	kthread_unpark(task);
	wake_up_process(task);
	wait_for_completion(&task->exited);

	retval = task->task_ret;
	linux_free_current(task);

	return (retval);
}

struct task_struct *
linux_kthread_setup_and_run(struct thread *td, linux_task_fn_t *task_fn, void *arg)
{
	struct task_struct *task;

	linux_set_current(td);

	task = td->td_lkpi_task;
	task->task_fn = task_fn;
	task->task_data = arg;

	/* make sure the scheduler priority is raised */
	thread_lock(td);
	sched_prio(td, PI_SWI(SWI_NET));
	sched_add(td, SRQ_BORING);
	thread_unlock(td);

	return (task);
}

void
linux_kthread_fn(void *arg __unused)
{
	struct task_struct *task;
	struct thread *td;

	td = curthread;
	task = current;

	if (kthread_should_stop())
		goto skip;

	task->task_ret = task->task_fn(task->task_data);

	PROC_LOCK(td->td_proc);
	wakeup(task);
	PROC_UNLOCK(td->td_proc);

	if (kthread_should_stop()) {
skip:
		/* let kthread_stop() free data */
		td->td_lkpi_task = NULL;
		/* wakeup kthread_stop() */
		complete(&task->exited);
	}
	kthread_exit();
}

int
linux_try_to_wake_up(struct task_struct *task, unsigned int state)
{
	int rc;

	rc = 0;
	if ((task->state & state) == 0)
		goto out;
	rc = 1;
	if (!TD_IS_RUNNING(task->task_thread)) {
		task->state = TASK_WAKING;
		wakeup_one(task);
	}
out:
	return (rc);
}

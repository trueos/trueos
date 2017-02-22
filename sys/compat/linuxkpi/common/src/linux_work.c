/*-
 * Copyright (c) 2017 Hans Petter Selasky
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/compat.h>

#include <sys/kernel.h>

/*
 * Define all work struct states
 */
enum {
	WORK_ST_IDLE,			/* idle - not started */
	WORK_ST_TIMER,			/* timer is being started */
	WORK_ST_TASK,			/* taskqueue is being queued */
	WORK_ST_EXEC,			/* callback is being called */
	WORK_ST_LOOP,			/* requeue is being requested */
	WORK_ST_CANCEL,			/* cancel is being requested */
	WORK_ST_MAX,
};

/*
 * Define global workqueues
 */
struct workqueue_struct *system_long_wq;
struct workqueue_struct *system_wq;
struct workqueue_struct *system_unbound_wq;
struct workqueue_struct *system_power_efficient_wq;

/*
 * This function atomically updates the work state and returns the
 * previous state at the time of update.
 */
static const uint8_t
linux_update_state(atomic_t *v, const uint8_t *pstate)
{
	int c, old;

	c = v->counter;

	while ((old = atomic_cmpxchg(v, c, pstate[c])) != c)
		c = old;

	return (c);
}

/*
 * A LinuxKPI task is allowed to free itself inside the callback function
 * and cannot safely be referred after the callback function has
 * completed. This function gives the linux_work_fn() function a hint,
 * that the task is not going away and can have its state checked
 * again. Without this extra hint LinuxKPI tasks cannot be serialized
 * accross multiple worker threads.
 */
static const bool
linux_work_exec_unblock(struct work_struct *work)
{
	struct workqueue_struct *wq;
	struct work_exec *exec;
	bool retval = 0;

	wq = work->work_queue;
	if (unlikely(wq == NULL))
		goto done;

	WQ_EXEC_LOCK(wq);
	TAILQ_FOREACH(exec, &wq->exec_head, entry) {
		if (exec->target == work) {
			exec->target = NULL;
			retval = 1;
			break;
		}
	}
	WQ_EXEC_UNLOCK(wq);
done:
	return (retval);
}

static void
linux_delayed_work_enqueue(struct delayed_work *dwork)
{
	struct taskqueue *tq;

	tq = dwork->work.work_queue->taskqueue, 
	taskqueue_enqueue(tq, &dwork->work.work_task);
}

/*
 * This function queues the given work structure on the given
 * workqueue. It returns non-zero if the work was successfully
 * [re-]queued. Else the work is already pending for completion.
 */
bool
queue_work_on(int cpu __unused, struct workqueue_struct *wq, struct work_struct *work)
{
	static const uint8_t states[WORK_ST_MAX] __aligned(8) = {
		[WORK_ST_IDLE] = WORK_ST_TASK,		/* start queuing task */
		[WORK_ST_TIMER] = WORK_ST_TIMER,	/* NOP */
		[WORK_ST_TASK] = WORK_ST_TASK,		/* NOP */
		[WORK_ST_EXEC] = WORK_ST_LOOP,		/* queue task another time */
		[WORK_ST_LOOP] = WORK_ST_LOOP,		/* NOP */
		[WORK_ST_CANCEL] = WORK_ST_CANCEL,	/* NOP */
	};

	if (atomic_read(&wq->draining) != 0)
		return (!work_pending(work));

	switch (linux_update_state(&work->state, states)) {
	case WORK_ST_EXEC:
		if (linux_work_exec_unblock(work))
			return (1);
		/* FALLTHROUGH */
	case WORK_ST_IDLE:
		work->work_queue = wq;
		taskqueue_enqueue(wq->taskqueue, &work->work_task);
		return (1);
	default:
		return (0);		/* already on a queue */
	}
}

/*
 * This function queues the given work structure on the given
 * workqueue after a given delay in ticks. It returns non-zero if the
 * work was successfully [re-]queued. Else the work is already pending
 * for completion.
 */
bool
queue_delayed_work_on(int cpu, struct workqueue_struct *wq,
    struct delayed_work *dwork, unsigned long delay)
{
	static const uint8_t states[WORK_ST_MAX] __aligned(8) = {
		[WORK_ST_IDLE] = WORK_ST_TIMER,		/* start timeout */
		[WORK_ST_TIMER] = WORK_ST_TIMER,	/* NOP */
		[WORK_ST_TASK] = WORK_ST_TASK,		/* NOP */
		[WORK_ST_EXEC] = WORK_ST_TIMER,		/* start timeout */
		[WORK_ST_LOOP] = WORK_ST_LOOP,		/* NOP */
		[WORK_ST_CANCEL] = WORK_ST_CANCEL,	/* NOP */
	};

	if (atomic_read(&wq->draining) != 0)
		return (!work_pending(&dwork->work));

	switch (linux_update_state(&dwork->work.state, states)) {
	case WORK_ST_EXEC:
		if (delay == 0 && linux_work_exec_unblock(&dwork->work) != 0)
			return (1);
		/* FALLTHROUGH */
	case WORK_ST_IDLE:
		dwork->work.work_queue = wq;
		dwork->cpu = cpu;
		dwork->timer.expires = ticks + delay;

		if (delay == 0)
			linux_delayed_work_enqueue(dwork);
		else if (unlikely(cpu != WORK_CPU_UNBOUND))
			add_timer_on(&dwork->timer, cpu);
		else
			add_timer(&dwork->timer);
		return (1);
	default:
		return (0);		/* already on a queue */
	}
}

void
linux_work_fn(void *context, int pending)
{
	static const uint8_t states[WORK_ST_MAX] __aligned(8) = {
		[WORK_ST_IDLE] = WORK_ST_IDLE,	/* NOP */
		[WORK_ST_TIMER] = WORK_ST_EXEC,	/* delayed work w/o timeout */
		[WORK_ST_TASK] = WORK_ST_EXEC,	/* start callback */
		[WORK_ST_EXEC] = WORK_ST_IDLE,	/* complete work */
		[WORK_ST_LOOP] = WORK_ST_TASK,	/* queue task another time */
		[WORK_ST_CANCEL] = WORK_ST_IDLE,/* complete cancel */
	};
	struct work_struct *work;
	struct workqueue_struct *wq;
	struct work_exec exec;

	MPASS(pending == 1);

	linux_set_current(curthread);

	/* setup local variables */
	work = context;
	wq = work->work_queue;

	/* store target pointer */
	exec.target = work;

	/* insert executor into list */
	WQ_EXEC_LOCK(wq);
	TAILQ_INSERT_TAIL(&wq->exec_head, &exec, entry);
	while (1) {
		switch (linux_update_state(&work->state, states)) {
		case WORK_ST_LOOP:
			break;
		case WORK_ST_TIMER:
		case WORK_ST_TASK:
			WQ_EXEC_UNLOCK(wq);

			/* call work function */
			work->func(work);

			WQ_EXEC_LOCK(wq);
			/* check if unblocked */
			if (exec.target != work) {
				/* reapply block */
				exec.target = work;
				break;
			}
			/* FALLTHROUGH */
		default:
			goto done;
		}
	}
done:
	/* remove executor from list */
	TAILQ_REMOVE(&wq->exec_head, &exec, entry);
	WQ_EXEC_UNLOCK(wq);
}

void
linux_delayed_work_timer_fn(unsigned long context)
{
	static const uint8_t states[WORK_ST_MAX] __aligned(8) = {
		[WORK_ST_IDLE] = WORK_ST_IDLE,	/* NOP */
		[WORK_ST_TIMER] = WORK_ST_TASK,	/* start queueing task */
		[WORK_ST_TASK] = WORK_ST_TASK,	/* NOP */
		[WORK_ST_EXEC] = WORK_ST_LOOP,	/* queue task another time */
		[WORK_ST_LOOP] = WORK_ST_LOOP,	/* NOP */
		[WORK_ST_CANCEL] = WORK_ST_IDLE,/* complete cancel */
	};
	struct delayed_work *dwork;

	dwork = (struct delayed_work *)context;

	switch (linux_update_state(&dwork->work.state, states)) {
	case WORK_ST_TIMER:
		linux_delayed_work_enqueue(dwork);
		break;
	default:
		break;
	}
}

/*
 * This function cancels the given work structure in a synchronous
 * fashion. It returns non-zero if the work was successfully
 * cancelled. Else the work was already cancelled.
 */
bool
cancel_work_sync(struct work_struct *work)
{
	static const uint8_t states[WORK_ST_MAX] __aligned(8) = {
		[WORK_ST_IDLE] = WORK_ST_IDLE,		/* NOP */
		[WORK_ST_TIMER] = WORK_ST_CANCEL,	/* cancel */
		[WORK_ST_TASK] = WORK_ST_CANCEL,	/* cancel */
		[WORK_ST_EXEC] = WORK_ST_CANCEL,	/* cancel */
		[WORK_ST_LOOP] = WORK_ST_CANCEL,	/* cancel */
		[WORK_ST_CANCEL] = WORK_ST_CANCEL,	/* NOP */
	};
	struct taskqueue *tq;

	might_sleep();

	switch (linux_update_state(&work->state, states)) {
	case WORK_ST_TASK:
	case WORK_ST_EXEC:
	case WORK_ST_LOOP:
		linux_work_exec_unblock(work);
		tq = work->work_queue->taskqueue;
		if (taskqueue_cancel(tq, &work->work_task, NULL))
			taskqueue_drain(tq, &work->work_task);
		break;
	default:
		return (0);
	}
	atomic_set(&work->state, WORK_ST_IDLE);
	return (1);
}

/*
 * This function cancels the given delayed work structure in a
 * non-blocking fashion. It returns non-zero if the work was
 * successfully cancelled. Else the work may still be busy or already
 * cancelled.
 */
bool
cancel_delayed_work(struct delayed_work *dwork)
{
	static const uint8_t states[WORK_ST_MAX] __aligned(8) = {
		[WORK_ST_IDLE] = WORK_ST_IDLE,		/* NOP */
		[WORK_ST_TIMER] = WORK_ST_CANCEL,	/* cancel */
		[WORK_ST_TASK] = WORK_ST_CANCEL,	/* cancel */
		[WORK_ST_EXEC] = WORK_ST_CANCEL,	/* cancel */
		[WORK_ST_LOOP] = WORK_ST_CANCEL,	/* cancel */
		[WORK_ST_CANCEL] = WORK_ST_CANCEL,	/* NOP */
	};
	struct taskqueue *tq;

	switch (linux_update_state(&dwork->work.state, states)) {
	case WORK_ST_TIMER:
		if (del_timer(&dwork->timer) != 0)
			break;
		/* FALLTHROUGH */
	case WORK_ST_TASK:
	case WORK_ST_EXEC:
	case WORK_ST_LOOP:
		linux_work_exec_unblock(&dwork->work);
		tq = dwork->work.work_queue->taskqueue;
		if (taskqueue_cancel(tq, &dwork->work.work_task, NULL) == 0)
			break;
		/* FALLTHROUGH */
	default:
		return (0);
	}
	atomic_set(&dwork->work.state, WORK_ST_IDLE);
	return (1);
}

/*
 * This function cancels the given work structure in a synchronous
 * fashion. It returns non-zero if the work was successfully
 * cancelled. Else the work was already cancelled.
 */
bool
cancel_delayed_work_sync(struct delayed_work *dwork)
{
	static const uint8_t states[WORK_ST_MAX] __aligned(8) = {
		[WORK_ST_IDLE] = WORK_ST_IDLE,		/* NOP */
		[WORK_ST_TIMER] = WORK_ST_CANCEL,	/* cancel */
		[WORK_ST_TASK] = WORK_ST_CANCEL,	/* cancel */
		[WORK_ST_EXEC] = WORK_ST_CANCEL,	/* cancel */
		[WORK_ST_LOOP] = WORK_ST_CANCEL,	/* cancel */
		[WORK_ST_CANCEL] = WORK_ST_CANCEL,	/* NOP */
	};
	struct taskqueue *tq;

	might_sleep();

	switch (linux_update_state(&dwork->work.state, states)) {
	case WORK_ST_TIMER:
		/* try to stop timer */
		if (del_timer(&dwork->timer) != 0)
			break;
		/* ensure timer callback has returned */
		(void)del_timer_sync(&dwork->timer);
		/* FALLTHROUGH */
	case WORK_ST_TASK:
	case WORK_ST_EXEC:
	case WORK_ST_LOOP:
	case WORK_ST_CANCEL:
		linux_work_exec_unblock(&dwork->work);
		tq = dwork->work.work_queue->taskqueue;
		if (taskqueue_cancel(tq, &dwork->work.work_task, NULL) != 0)
			taskqueue_drain(tq, &dwork->work.work_task);
		break;
	default:
		return (0);
	}
	atomic_set(&dwork->work.state, WORK_ST_IDLE);
	return (1);
}

/*
 * This function waits until the given work structure is completed.
 * It returns non-zero if the work was successfully
 * waited for. Else the work was not waited for.
 */
bool
flush_work(struct work_struct *work)
{
	struct taskqueue *tq;

	switch (atomic_read(&work->state)) {
	case WORK_ST_TASK:
	case WORK_ST_EXEC:
	case WORK_ST_LOOP:
	case WORK_ST_CANCEL:
		linux_work_exec_unblock(work);
		tq = work->work_queue->taskqueue;
		taskqueue_drain(tq, &work->work_task);
		return (1);
	default:
		return (0);
	}
}

/*
 * This function waits until the given delayed work structure is
 * completed. It returns non-zero if the work was successfully waited
 * for. Else the work was not waited for.
 */
bool
flush_delayed_work(struct delayed_work *dwork)
{
	static const uint8_t states[WORK_ST_MAX] __aligned(8) = {
		[WORK_ST_IDLE] = WORK_ST_IDLE,		/* NOP */
		[WORK_ST_TIMER] = WORK_ST_TASK,		/* kick start task */
		[WORK_ST_TASK] = WORK_ST_TASK,		/* NOP */
		[WORK_ST_EXEC] = WORK_ST_EXEC,		/* NOP */
		[WORK_ST_LOOP] = WORK_ST_LOOP,		/* NOP */
		[WORK_ST_CANCEL] = WORK_ST_CANCEL,	/* NOP */
	};
	struct taskqueue *tq;

	switch (linux_update_state(&dwork->work.state, states)) {
	case WORK_ST_TIMER:
		(void)del_timer_sync(&dwork->timer);
		linux_delayed_work_enqueue(dwork);
		/* FALLTHROUGH */
	case WORK_ST_TASK:
	case WORK_ST_EXEC:
	case WORK_ST_LOOP:
	case WORK_ST_CANCEL:
		linux_work_exec_unblock(&dwork->work);
		tq = dwork->work.work_queue->taskqueue;
		taskqueue_drain(tq, &dwork->work.work_task);
		return (1);
	default:
		return (0);
	}
}

/*
 * This function returns true if the given work is pending, and not
 * yet executing:
 */
bool
work_pending(struct work_struct *work)
{
	switch (atomic_read(&work->state)) {
	case WORK_ST_TIMER:
	case WORK_ST_TASK:
	case WORK_ST_LOOP:
		return (1);
	default:
		return (0);
	}
}

/*
 * This function returns true if the given work is busy.
 */
bool
work_busy(struct work_struct *work)
{
	struct taskqueue *tq;

	switch (atomic_read(&work->state)) {
	case WORK_ST_IDLE:
		return (0);
	case WORK_ST_EXEC:
	case WORK_ST_CANCEL:
		tq = work->work_queue->taskqueue;
		return (taskqueue_is_pending_or_running(tq, &work->work_task));
	default:
		return (1);
	}
}

void
linux_flush_fn(void *context, int pending)
{
	MPASS(pending == 1);
}

struct workqueue_struct *
linux_create_workqueue_common(const char *name, int cpus)
{
	struct workqueue_struct *wq;

	wq = kmalloc(sizeof(*wq), M_WAITOK | M_ZERO);
	wq->taskqueue = taskqueue_create(name, M_WAITOK,
	    taskqueue_thread_enqueue, &wq->taskqueue);
	atomic_set(&wq->draining, 0);
	taskqueue_start_threads(&wq->taskqueue, cpus, PWAIT, "%s", name);
	TAILQ_INIT(&wq->exec_head);
	mtx_init(&wq->exec_mtx, "linux_wq_exec", NULL, MTX_DEF);

	return (wq);
}

void
destroy_workqueue(struct workqueue_struct *wq)
{
	atomic_inc(&wq->draining);
	drain_workqueue(wq);
	taskqueue_free(wq->taskqueue);
	mtx_destroy(&wq->exec_mtx);
	kfree(wq);
}

static void
linux_work_init(void *arg)
{
	const int max_wq_cpus = mp_ncpus + 1 /* avoid deadlock for !SMP */;

	system_long_wq = alloc_workqueue("events_long", 0, max_wq_cpus);
	system_wq = alloc_workqueue("events", 0, max_wq_cpus);
	system_power_efficient_wq = alloc_workqueue("power efficient", 0, max_wq_cpus);
	system_unbound_wq = alloc_workqueue("events_unbound", WQ_UNBOUND, max_wq_cpus);
}
SYSINIT(linux_work_init, SI_SUB_LOCK, SI_ORDER_SECOND, linux_work_init, NULL);

static void
linux_work_uninit(void *arg)
{

	destroy_workqueue(system_long_wq);
	destroy_workqueue(system_wq);
	destroy_workqueue(system_power_efficient_wq);
	destroy_workqueue(system_unbound_wq);

	system_long_wq = NULL;
	system_wq = NULL;
	system_power_efficient_wq = NULL;
	system_unbound_wq = NULL;
}
SYSUNINIT(linux_work_uninit, SI_SUB_LOCK, SI_ORDER_SECOND, linux_work_uninit, NULL);

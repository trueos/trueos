
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/compat.h>

static void
set_work_data(struct work_struct *work, unsigned long data,
				 unsigned long flags)
{
	WARN_ON_ONCE(!work_pending(work));
	atomic_long_set(&work->data, data | flags);
}

static void
set_work_pool_and_clear_pending(struct work_struct *work,
					    int pool_id)
{
	smp_wmb();
	set_work_data(work, (unsigned long)pool_id << WORK_OFFQ_POOL_SHIFT, 0);
	smp_wmb();
}

static void
clear_work_data(struct work_struct *work)
{
	smp_wmb();	/* see set_work_pool_and_clear_pending() */
	set_work_data(work, WORK_STRUCT_NO_POOL, 0);
}

static int
get_work_pool_id(struct work_struct *work)
{
	unsigned long data = atomic_long_read(&work->data);

#ifdef __notyet__
	if (data & WORK_STRUCT_PWQ)
		return ((struct pool_workqueue *)
			(data & WORK_STRUCT_WQ_DATA_MASK))->pool->id;
#endif

	return data >> WORK_OFFQ_POOL_SHIFT;
}

static void
mark_work_canceling(struct work_struct *work)
{
	unsigned long pool_id = get_work_pool_id(work);

	pool_id <<= WORK_OFFQ_POOL_SHIFT;
	set_work_data(work, pool_id | WORK_OFFQ_CANCELING, WORK_STRUCT_PENDING);
}

static bool
work_is_canceling(struct work_struct *work)
{
	unsigned long data = atomic_long_read(&work->data);

	return !(data & WORK_STRUCT_PWQ) && (data & WORK_OFFQ_CANCELING);
}


static int
remove_task(struct work_struct *work, int delayed)
{
	struct delayed_work *dwork = to_delayed_work(work);

	if (delayed && __predict_true(del_timer(&dwork->timer)))
		return (1);

	if (!test_and_set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work)))
		return (0);
	flush_work(work);
	if (work_is_canceling(work))
		return -ENOENT;
	return (0);
}


struct linux_cwt_wait {
	wait_queue_t		wait;
	struct work_struct	*work;
};

static int
cwt_wakefn(wait_queue_t *wait, unsigned mode, int sync, void *key)
{
	struct linux_cwt_wait *cwait = container_of(wait, struct linux_cwt_wait, wait);

	if (cwait->work != key)
		return 0;
	return autoremove_wake_function(wait, mode, sync, key);
}

static int
linux_cancel_work_timer(struct work_struct *work, int delayed)
{
	static DECLARE_WAIT_QUEUE_HEAD(wq);
	int rc;

	do {
		rc = remove_task(work, delayed);
		if (__predict_false(rc == -ENOENT)) {
			struct linux_cwt_wait cwait;

			init_wait(&cwait.wait);
			cwait.wait.func = cwt_wakefn;
			cwait.work = work;

			prepare_to_wait_exclusive(&wq, &cwait.wait,
						  TASK_UNINTERRUPTIBLE);
			if (work_is_canceling(work))
				schedule();
			finish_wait(&wq, &cwait.wait);
		}
	} while (__predict_false(rc < 0));

	mark_work_canceling(work);

	flush_work(work);
	clear_work_data(work);

	smp_mb();
	if (waitqueue_active(&wq))
		__wake_up(&wq, TASK_NORMAL, 1, work);

	return (rc);
}

void
linux_work_fn(void *context, int pending)
{
	struct work_struct *work;

	work = context;

	linux_set_current();
	set_work_pool_and_clear_pending(work, 0);
	work->fn(work);
}

void
linux_flush_fn(void *context, int pending)
{
}

struct workqueue_struct *
linux_create_workqueue_common(const char *name, int cpus)
{
	struct workqueue_struct *wq;

	wq = kmalloc(sizeof(*wq), M_WAITOK);
	wq->taskqueue = taskqueue_create_fast(name, M_WAITOK,
	    taskqueue_thread_enqueue,  &wq->taskqueue);
	atomic_set(&wq->draining, 0);
	taskqueue_start_threads(&wq->taskqueue, cpus, PWAIT, "%s", name);

	return (wq);
}

void
destroy_workqueue(struct workqueue_struct *wq)
{
	atomic_inc(&wq->draining);
	drain_workqueue(wq);
	taskqueue_free(wq->taskqueue);
	kfree(wq);
}

void
linux_queue_work(int cpu __unused, struct workqueue_struct *wq, struct work_struct *work)
{
	work->taskqueue = wq->taskqueue;
	taskqueue_enqueue(wq->taskqueue, &work->work_task);
}

bool
queue_work_on(int cpu, struct workqueue_struct *wq, struct work_struct *work)
{
	bool ret = false;

	/* XXX fix CPU usage */
	if (!test_and_set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work))) {
		linux_queue_work(cpu, wq, work);
		ret = true;
	}
	return (ret);
}

void
linux_delayed_work_timer_fn(unsigned long __data)
{
	struct delayed_work *dwork = (struct delayed_work *)__data;

	linux_queue_work(dwork->cpu, dwork->wq, &dwork->work);
}

static void
linux_queue_delayed_work(int cpu, struct workqueue_struct *wq, struct delayed_work *dwork,
			 unsigned long delay)
{
	struct timer_list *timer;

	if (delay == 0) {
		linux_queue_work(cpu, wq, &dwork->work);
		return;
	}
	timer = &dwork->timer;
	dwork->wq = wq;
	dwork->cpu = cpu;
	timer->expires = ticks + delay;

	if (unlikely(cpu != WORK_CPU_UNBOUND))
		add_timer_on(timer, cpu);
	else
		add_timer(timer);

}

bool
queue_delayed_work_on(int cpu, struct workqueue_struct *wq,
			   struct delayed_work *dwork, unsigned long delay)
{
	struct work_struct *work = &dwork->work;
	bool rc = false;

	if (!test_and_set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work))) {
		work->taskqueue = wq->taskqueue;
		linux_queue_delayed_work(cpu, wq, dwork, delay);
		rc = true;
	}

	return (rc);
}

int
cancel_work_sync(struct work_struct *work)
{
	if (work->taskqueue &&
	    taskqueue_cancel(work->taskqueue, &work->work_task, NULL))
		taskqueue_drain(work->taskqueue, &work->work_task);

	return (linux_cancel_work_timer(work, 0));
}

bool
cancel_delayed_work(struct delayed_work *dwork)
{
	int rc;

	rc = remove_task(&dwork->work, true);

	if (unlikely(rc < 0))
		return false;

	set_work_pool_and_clear_pending(&dwork->work,
					get_work_pool_id(&dwork->work));
	return (rc);
}

int
cancel_delayed_work_sync(struct delayed_work *work)
{
	while (work->work.taskqueue &&
	    taskqueue_cancel(work->work.taskqueue, &work->work.work_task,
	    NULL) != 0)
		taskqueue_drain(work->work.taskqueue, &work->work.work_task);

	return (linux_cancel_work_timer(&work->work, 1));
}

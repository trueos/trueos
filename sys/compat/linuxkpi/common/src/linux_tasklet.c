#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/gtaskqueue.h>

#include <linux/interrupt.h>
struct tasklet_head {
	struct tasklet_struct *head;
	struct tasklet_struct **tail;
};
DPCPU_DEFINE(struct tasklet_head, tasklet_head);

#ifdef INVARIANTS
static int tasklet_schedule_cnt, tasklet_handler_cnt, tasklet_func_cnt;
#endif
static struct grouptask_aligned *tasklet_gtask_array;

static void
tasklet_handler(void *arg)
{
	struct tasklet_struct *t, *l;
	struct grouptask *gtask;
	struct tasklet_head *h;
#ifdef INVARIANTS
	intptr_t cpuid = (intptr_t)arg;
#endif

	MPASS(curcpu == cpuid);

	disable_intr();
	h = &DPCPU_GET(tasklet_head);
	l = h->head;
	h->head = NULL;
	h->tail = &h->head;
	enable_intr();

#ifdef INVARIANTS
	atomic_add_int(&tasklet_handler_cnt, 1);
#endif	
	while (l) {
		t = l;
		l = l->next;

		if (tasklet_trylock(t)) {
#ifdef INVARIANTS	
			atomic_add_int(&tasklet_func_cnt, 1);
#endif
			if (!atomic_read(&t->count)) {
					if (!test_and_clear_bit(TASKLET_STATE_SCHED,
							&t->state))
						BUG();
				t->func(t->data);
				tasklet_unlock(t);
				continue;
			}
			tasklet_unlock(t);
		}
		disable_intr();
		t->next = NULL;
		*(h->tail) = t;
		h->tail = &t->next;
		gtask = (struct grouptask *)&tasklet_gtask_array[curcpu];
		GROUPTASK_ENQUEUE(gtask);
		enable_intr();
	}
}

static void
tasklet_subsystem_init(void *arg __unused)
{
	int i;
	struct grouptask *gtask;
	struct tasklet_head *head;
	char buf[32];

	tasklet_gtask_array = malloc(sizeof(struct grouptask_aligned)*mp_ncpus, M_KMALLOC, M_WAITOK|M_ZERO);

	CPU_FOREACH(i) {
		if (CPU_ABSENT(i))
			continue;
		head = DPCPU_ID_PTR(i, tasklet_head);
		head->tail = &head->head;

		gtask = (struct grouptask *)&tasklet_gtask_array[i];
		GROUPTASK_INIT(gtask, 0, tasklet_handler, (void *)(uintptr_t)i);
		snprintf(buf, 31, "softirq%d", i);
		taskqgroup_attach_cpu(qgroup_softirq, gtask, "tasklet", i, -1, buf);
	}
}
SYSINIT(linux_tasklet, SI_SUB_KTHREAD_PAGE, SI_ORDER_SECOND, tasklet_subsystem_init, NULL);

void
tasklet_init(struct tasklet_struct *t, void (*func)(unsigned long), unsigned long data)
{
	t->next = NULL;
	t->state = 0;
	atomic_set(&t->count, 0);
	t->func = func;
	t->data = data;	
}

void
__tasklet_schedule(struct tasklet_struct *t)
{
	struct tasklet_head *head;
	struct grouptask *gtask;
	int inintr;

	t->next = NULL;
	inintr = curthread->td_intr_nesting_level;

	if (!inintr)
		disable_intr();
	gtask = (struct grouptask *)&tasklet_gtask_array[curcpu];
	head = &DPCPU_GET(tasklet_head);
	*(head->tail) = t;
	head->tail = &(t->next);
	if (!inintr)
		enable_intr();
	GROUPTASK_ENQUEUE(gtask);
#ifdef INVARIANTS	
	atomic_add_int(&tasklet_schedule_cnt, 1);
#endif	
}

void
tasklet_kill(struct tasklet_struct *t)
{
	while (test_and_set_bit(TASKLET_STATE_SCHED, &t->state)) {
		do {
			kern_yield(0);
		} while (test_bit(TASKLET_STATE_SCHED, &t->state));
	}
	tasklet_unlock_wait(t);
	clear_bit(TASKLET_STATE_SCHED, &t->state);	
}


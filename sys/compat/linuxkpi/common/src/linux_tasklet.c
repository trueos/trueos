#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/gtaskqueue.h>

#include <linux/interrupt.h>
struct aligned_ptr {
	void *ptr;
} __aligned(CACHE_LINE_SIZE);

static int tasklet_schedule_cnt, tasklet_handler_cnt, tasklet_func_cnt;
static struct grouptask_aligned *tasklet_gtask_array;
static struct aligned_ptr *tasklet_head_array;

static void
tasklet_handler(void *arg)
{
	intptr_t cpuid = (intptr_t)arg;
	struct tasklet_struct *t, *tprev;

	MPASS(curcpu == cpuid);
	disable_intr();
	t = tasklet_head_array[cpuid].ptr;
	tasklet_head_array[cpuid].ptr = NULL;
	enable_intr();
#ifdef INVARIANTS
	atomic_add_int(&tasklet_handler_cnt, 1);
#endif	
	while (t != NULL) {
		if (tasklet_trylock(t)) {
#ifdef INVARIANTS	
			atomic_add_int(&tasklet_func_cnt, 1);
#endif			
			t->func(t->data);
			tprev = t;
			t = t->next;
			tasklet_unlock(tprev);
		} else
			t = t->next;
	}
}

static void
tasklet_subsystem_init(void *arg __unused)
{
	int i;
	struct grouptask *gtask;
	char buf[32];

	tasklet_gtask_array = malloc(sizeof(struct grouptask_aligned)*mp_ncpus, M_KMALLOC, M_WAITOK|M_ZERO);
	tasklet_head_array =  malloc(CACHE_LINE_SIZE*mp_ncpus, M_KMALLOC, M_WAITOK|M_ZERO);

	for (i = 0; i < mp_ncpus; i++) {
		if (CPU_ABSENT(i))
			continue;
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
	struct tasklet_struct *ttmp;
	struct grouptask *gtask;
	int cpuid, inintr;

	inintr = curthread->td_intr_nesting_level;

	if (!inintr)
		disable_intr();
	cpuid = curcpu;
	gtask = (struct grouptask *)&tasklet_gtask_array[cpuid];
	ttmp = tasklet_head_array[cpuid].ptr;
	t->next = ttmp;
	tasklet_head_array[cpuid].ptr = t;
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


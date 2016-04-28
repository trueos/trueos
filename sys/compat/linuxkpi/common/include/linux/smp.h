#ifndef __LINUX_SMP_H
#define __LINUX_SMP_H


#define get_cpu()		({ preempt_disable(); smp_processor_id(); })
#define put_cpu()		preempt_enable()
#endif

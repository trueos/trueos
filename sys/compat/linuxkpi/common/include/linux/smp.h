#ifndef __LINUX_SMP_H
#define __LINUX_SMP_H


#define get_cpu()		({ preempt_disable(); smp_processor_id(); })
#define put_cpu()		preempt_enable()


typedef void smp_call_func_t(void *info);

int on_each_cpu(smp_call_func_t func, void *info, int wait);

#endif

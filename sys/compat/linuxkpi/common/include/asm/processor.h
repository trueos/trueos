#ifndef _ASM_X86_PROCESSOR_H
#define _ASM_X86_PROCESSOR_H


#define	mb()	__asm __volatile("mfence;" : : : "memory")
#define	wmb()	__asm __volatile("sfence;" : : : "memory")
#define	rmb()	__asm __volatile("lfence;" : : : "memory")

#define smp_mb() mb()
#define smp_wmb() wmb()
#define smp_rmb() rmb()

/* REP NOP (PAUSE) is a good thing to insert into busy-wait loops. */
static __always_inline void rep_nop(void)
{
	__asm __volatile("rep; nop" ::: "memory");
}

static __always_inline void cpu_relax(void)
{
	rep_nop();
}

#define cpu_relax_lowlatency() cpu_relax()

#define smp_read_barrier_depends() do {} while (0)
#endif

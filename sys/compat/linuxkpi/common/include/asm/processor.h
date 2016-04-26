#ifndef _ASM_X86_PROCESSOR_H
#define _ASM_X86_PROCESSOR_H

/* REP NOP (PAUSE) is a good thing to insert into busy-wait loops. */
static __always_inline void rep_nop(void)
{
	__asm __volatile("rep; nop" ::: "memory");
}

static __always_inline void cpu_relax(void)
{
	rep_nop();
}

#define smp_read_barrier_depends() do {} while (0)
#endif

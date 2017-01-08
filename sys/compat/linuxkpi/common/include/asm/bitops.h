#if defined(__amd64__)
#ifndef _ASM_X86_BITOPS_H
#define _ASM_X86_BITOPS_H

#ifndef _LINUX_BITOPS_H
#error only <linux/bitops.h> can be included directly
#endif

#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 1)
/* Technically wrong, but this avoids compilation errors on some gcc
   versions. */
#define BITOP_ADDR(x) "=m" (*(volatile long *) (x))
#else
#define BITOP_ADDR(x) "+m" (*(volatile long *) (x))
#endif

#define ADDR				BITOP_ADDR(addr)

#ifdef __GCC_ASM_FLAG_OUTPUTS__
# define CC_SET(c) "\n\t/* output condition code " #c "*/\n"
# define CC_OUT(c) "=@cc" #c
#else
# define CC_SET(c) "\n\tset" #c " %[_cc_" #c "]\n"
# define CC_OUT(c) [_cc_ ## c] "=qm"
#endif
static inline bool
__test_and_set_bit(long nr, volatile unsigned long *addr)
{
	bool oldbit;

	__asm__("bts %2,%1\n\t"
	    CC_SET(c)
	    : CC_OUT(c) (oldbit), ADDR
	    : "Ir" (nr));
	return oldbit;
}

#endif
#endif

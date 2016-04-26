#ifndef _ASM_X86_MTRR_H
#define _ASM_X86_MTRR_H

extern int mtrr_add(unsigned long base, unsigned long size,
		    unsigned int type, bool increment);
extern int mtrr_del(int reg, unsigned long base, unsigned long size);

#define MTRR_TYPE_UNCACHABLE 0
#define MTRR_TYPE_WRCOMB     1

#define MTRR_TYPE_WRTHROUGH  4
#define MTRR_TYPE_WRPROT     5
#define MTRR_TYPE_WRBACK     6
#define MTRR_NUM_TYPES       7

#define arch_phys_wc_add(base, size) mtrr_add(base, size, MTRR_TYPE_WRCOMB, 1)
#define arch_phys_wc_del(reg, base, size) mtrr_del(reg, base, size)
#define arch_phys_wc_index(x) (x)

#endif

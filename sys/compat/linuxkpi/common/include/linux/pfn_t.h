#ifndef _LINUX_PFN_T_H_
#define _LINUX_PFN_T_H_
#include <linux/mm.h>


/*
 * PFN_FLAGS_MASK - mask of all the possible valid pfn_t flags
 * PFN_SG_CHAIN - pfn is a pointer to the next scatterlist entry
 * PFN_SG_LAST - pfn references a page and is the last scatterlist entry
 * PFN_DEV - pfn is not covered by system memmap by default
 * PFN_MAP - pfn has a dynamic page mapping established by a device driver
 */
#define PFN_FLAGS_MASK (((u64) ~PAGE_MASK) << (BITS_PER_LONG_LONG - PAGE_SHIFT))
#define PFN_SG_CHAIN (1ULL << (BITS_PER_LONG_LONG - 1))
#define PFN_SG_LAST (1ULL << (BITS_PER_LONG_LONG - 2))
#define PFN_DEV (1ULL << (BITS_PER_LONG_LONG - 3))
#define PFN_MAP (1ULL << (BITS_PER_LONG_LONG - 4))

static inline pfn_t __pfn_to_pfn_t(unsigned long pfn, u64 flags)
{
	pfn_t pfn_t = { .val = pfn | (flags & PFN_FLAGS_MASK), };

	return pfn_t;
}

/* a default pfn to pfn_t conversion assumes that @pfn is pfn_valid() */
static inline pfn_t pfn_to_pfn_t(unsigned long pfn)
{
	return __pfn_to_pfn_t(pfn, 0);
}


#endif

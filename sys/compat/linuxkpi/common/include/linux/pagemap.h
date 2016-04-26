#ifndef _LINUX_PAGEMAP_H
#define _LINUX_PAGEMAP_H



#define __get_user(a, b) ({panic("XXX implement me!!!"); NULL;})

static inline int fault_in_multipages_readable(const char __user *uaddr,
					       int size)
{
	volatile char c;
	int ret = 0;
	const char __user *end = uaddr + size - 1;

	if (unlikely(size == 0))
		return ret;

	while (uaddr <= end) {
		ret = __get_user(c, uaddr);
		if (ret != 0)
			return ret;
		uaddr += PAGE_SIZE;
	}

	/* Check whether the range spilled into the next page. */
	if (((unsigned long)uaddr & PAGE_MASK) ==
			((unsigned long)end & PAGE_MASK)) {
		ret = __get_user(c, end);
		(void)c;
	}

	return ret;
}

#endif

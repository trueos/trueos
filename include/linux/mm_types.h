#ifndef _LINUX_MM_TYPES_H
#define _LINUX_MM_TYPES_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/page.h>

#include <linux/rwsem.h>


struct mm_struct {
	struct vm_area_struct *mmap;		/* list of VMAs */
	atomic_t mm_count;			/* How many references to "struct mm_struct" (users count as 1) */
	atomic_t mm_users;
	struct rw_semaphore mmap_sem;

};


#endif

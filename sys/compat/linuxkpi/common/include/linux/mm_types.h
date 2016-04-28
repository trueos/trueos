#ifndef _LINUX_MM_TYPES_H
#define _LINUX_MM_TYPES_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/page.h>

#include <linux/mutex.h>


struct mm_struct {
	struct vm_area_struct *mmap;		/* list of VMAs */
	atomic_t mm_count;			/* How many references to "struct mm_struct" (users count as 1) */
	atomic_t mm_users;
	struct sx mmap_sem;
};

#define down_write(m) sx_xlock(m)
#define up_write(m) sx_xunlock(m)

#define down_read(m) sx_slock(m)
#define up_read(m) sx_sunlock(m)


#endif

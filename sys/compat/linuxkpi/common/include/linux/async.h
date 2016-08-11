#ifndef _LINUX_ASYNC_H_
#define _LINUX_ASYNC_H_

#include <linux/types.h>
#include <linux/list.h>
#include <linux/workqueue.h>

typedef u64 async_cookie_t;

typedef void (*async_func_t) (void *data, async_cookie_t cookie);

struct async_domain {
	struct list_head pending;
	unsigned registered:1;
};

struct async_entry {
	struct list_head	domain_list;
	struct list_head	global_list;
	struct work_struct	work;
	async_cookie_t		cookie;
	async_func_t		func;
	void			*data;
	struct async_domain	*domain;
};

extern async_cookie_t async_schedule(async_func_t func, void *data);
extern void async_synchronize_full(void);

static inline bool
current_is_async(void)
{
	UNIMPLEMENTED();
	return (false);
}
#endif

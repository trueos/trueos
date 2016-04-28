#ifndef _LINUX_ASYNC_H_
#define _LINUX_ASYNC_H_

#include <linux/types.h>
#include <linux/list.h>

typedef u64 async_cookie_t;

typedef void (*async_func_t) (void *data, async_cookie_t cookie);

struct async_domain {
	struct list_head pending;
	unsigned registered:1;
};

extern async_cookie_t async_schedule(async_func_t func, void *data);
extern void async_synchronize_full(void);
extern bool current_is_async(void);

#endif

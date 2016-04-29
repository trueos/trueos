/*-
 * Copyright (c) 2015 François Tigeot
 * Copyright (c) 2016 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _LINUX_RCULIST_H_
#define _LINUX_RCULIST_H_

#include <linux/list.h>

struct hlist_node;

static inline void hlist_add_behind_rcu(struct hlist_node *n,
				        struct hlist_node *prev)
{
	hlist_add_after(prev, n);
}

#define hlist_for_each_entry_rcu(pos, head, member)	\
	hlist_for_each_entry(pos, head, member)

#define	hlist_add_head_rcu(n, h)		\
do {						\
  	sx_xlock(&linux_global_rcu_lock);	\
	hlist_add_head(n, h);			\
	sx_xunlock(&linux_global_rcu_lock);	\
} while (0)

#define	hlist_del_init_rcu(n)			\
do {						\
    	sx_xlock(&linux_global_rcu_lock);	\
	hlist_del_init(n);			\
	sx_xunlock(&linux_global_rcu_lock);	\
} while (0)

#define	hlist_del_rcu(n)			\
do {						\
    	sx_xlock(&linux_global_rcu_lock);	\
	hlist_del(n);				\
	sx_xunlock(&linux_global_rcu_lock);	\
} while (0)


#endif	/* _LINUX_RCULIST_H_ */

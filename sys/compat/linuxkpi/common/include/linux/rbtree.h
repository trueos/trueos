/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
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
#ifndef	_LINUX_RBTREE_H_
#define	_LINUX_RBTREE_H_

#include <sys/stddef.h>
#include <sys/types.h>
#include <sys/tree.h>

struct rb_node {
	RB_ENTRY(rb_node)	__entry;
};
#define	rb_left		__entry.rbe_left
#define	rb_right	__entry.rbe_right

/*
 * We provide a false structure that has the same bit pattern as tree.h
 * presents so it matches the member names expected by linux.
 */
struct rb_root {
	struct	rb_node	*rb_node;
};

/*
 * In linux all of the comparisons are done by the caller.
 */
int panic_cmp(struct rb_node *one, struct rb_node *two);

RB_HEAD(linux_root, rb_node);
RB_PROTOTYPE(linux_root, rb_node, __entry, panic_cmp);

#define	rb_parent(r)	RB_PARENT(r, __entry)
#define	rb_color(r)	RB_COLOR(r, __entry)
#define	rb_is_red(r)	(rb_color(r) == RB_RED)
#define	rb_is_black(r)	(rb_color(r) == RB_BLACK)
#define	rb_set_parent(r, p)	rb_parent((r)) = (p)
#define	rb_set_color(r, c)	rb_color((r)) = (c)
#define	rb_entry(ptr, type, member)	container_of(ptr, type, member)

#define RB_EMPTY_ROOT(root)     (((struct linux_root *)root)->rbh_root == NULL)
#define RB_EMPTY_NODE(node)     (rb_parent(node) == node)
#define RB_CLEAR_NODE(node)     (rb_set_parent(node, node))

#define	rb_insert_color(node, root)					\
	linux_root_RB_INSERT_COLOR((struct linux_root *)(root), (node))
#define	rb_erase(node, root)						\
	linux_root_RB_REMOVE((struct linux_root *)(root), (node))
#define	rb_next(node)	RB_NEXT(linux_root, NULL, (node))
#define	rb_prev(node)	RB_PREV(linux_root, NULL, (node))
#define	rb_first(root)	RB_MIN(linux_root, (struct linux_root *)(root))
#define	rb_last(root)	RB_MAX(linux_root, (struct linux_root *)(root))

static inline void
rb_link_node(struct rb_node *node, struct rb_node *parent,
    struct rb_node **rb_link)
{
	rb_set_parent(node, parent);
	rb_set_color(node, RB_RED);
	node->__entry.rbe_left = node->__entry.rbe_right = NULL;
	*rb_link = node;
}

static inline void
rb_replace_node(struct rb_node *victim, struct rb_node *new,
    struct rb_root *root)
{
	struct rb_node *p;

	p = rb_parent(victim);
	if (p) {
		if (p->rb_left == victim)
			p->rb_left = new;
		else
			p->rb_right = new;
	} else
		root->rb_node = new;
	if (victim->rb_left)
		rb_set_parent(victim->rb_left, new);
	if (victim->rb_right)
		rb_set_parent(victim->rb_right, new);
	*new = *victim;
}

#undef RB_ROOT
#define RB_ROOT		(struct rb_root) { NULL }



static inline struct rb_node *
rb_leftmost_leaf(const struct rb_node *node)
{
	for (;;) {
		if (node->rb_left)
			node = node->rb_left;
		else if (node->rb_right)
			node = node->rb_right;
		else
			return ((struct rb_node *)(uintptr_t)node);
	}
}

static inline struct rb_node *
rb_first_postorder(const struct rb_root *root)
{
	if (root->rb_node == NULL)
		return (NULL);
	return (rb_leftmost_leaf(root->rb_node));
}

static inline struct rb_node *
rb_next_postorder(const struct rb_node *node)
{
	const struct rb_node *parent;

	if (node == NULL)
		return (NULL);

	parent = rb_parent(node);
	if (parent && node == parent->rb_left && parent->rb_right) {
		return (rb_leftmost_leaf(parent->rb_right));
	} else
		return ((struct rb_node *)(uintptr_t)parent);
}

#define rb_entry_safe(ptr, type, member) \
	({ typeof(ptr) ____ptr = (ptr); \
	   ____ptr ? rb_entry(____ptr, type, member) : NULL; \
	})

#define rbtree_postorder_for_each_entry_safe(pos, n, root, field)	\
	for (pos = rb_entry_safe(rb_first_postorder(root), typeof(*pos), field); \
	     pos && ({ n = rb_entry_safe(rb_next_postorder(&pos->field), \
			typeof(*pos), field); 1; }); \
	     pos = n)



#endif	/* _LINUX_RBTREE_H_ */

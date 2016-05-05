#ifndef _LINUX_INTERVAL_TREE_H_
#define _LINUX_INTERVAL_TREE_H_

#include <linux/rbtree.h>

struct interval_tree_node {
	struct rb_node rb;
        unsigned long start;    /* Start of interval */
        unsigned long last;     /* Last location _in_ interval */
        unsigned long __subtree_last;
};

#endif

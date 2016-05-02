#ifndef _LINUX_OF_H
#define _LINUX_OF_H

struct device_node {
	const char *name;
	const char *type;
};

static inline struct device_node *of_node_get(struct device_node *node)
{
	return node;
}
static inline void of_node_put(struct device_node *node) { }

#endif

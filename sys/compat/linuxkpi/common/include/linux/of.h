#ifndef _LINUX_OF_H
#define _LINUX_OF_H


#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/kobject.h>
#include <linux/mod_devicetable.h>
#include <linux/spinlock.h>
#include <linux/notifier.h>
#include <linux/property.h>
#include <linux/list.h>

struct device_node {
	const char *name;
	const char *type;
	struct fwnode_handle fwnode;

};

static inline struct device_node *of_node_get(struct device_node *node)
{
	return node;
}
static inline void of_node_put(struct device_node *node) { }

#endif

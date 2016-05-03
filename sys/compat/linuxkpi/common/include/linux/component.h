#ifndef COMPONENT_H
#define COMPONENT_H

#include <linux/stddef.h>

struct device;

struct component_ops {
	int (*bind)(struct device *comp, struct device *master,
		    void *master_data);
	void (*unbind)(struct device *comp, struct device *master,
		       void *master_data);
};

static inline int
component_add(struct device *dev, const struct component_ops *ops)
{
	UNIMPLEMENTED();
	return (0);
}

static inline void
component_del(struct device *dev, const struct component_ops *ops)
{
	UNIMPLEMENTED();
}

int component_bind_all(struct device *master, void *master_data);
void component_unbind_all(struct device *master, void *master_data);

struct master;

struct component_master_ops {
	int (*bind)(struct device *master);
	void (*unbind)(struct device *master);
};

static inline void
component_master_del(struct device *dev, const struct component_master_ops *ops)
{
	UNIMPLEMENTED();
}

struct component_match;

int component_master_add_with_match(struct device *,
	const struct component_master_ops *, struct component_match *);
void component_match_add_release(struct device *master,
	struct component_match **matchptr,
	void (*release)(struct device *, void *),
	int (*compare)(struct device *, void *), void *compare_data);

static inline void component_match_add(struct device *master,
	struct component_match **matchptr,
	int (*compare)(struct device *, void *), void *compare_data)
{
	component_match_add_release(master, matchptr, NULL, compare,
				    compare_data);
}

#endif

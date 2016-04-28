#ifndef _LINUX_IOPORT_H
#define _LINUX_IOPORT_H



extern struct linux_resource ioport_resource;
extern struct linux_resource iomem_resource;

struct linux_resource {
	resource_size_t start;
	resource_size_t end;
	const char *name;
	unsigned long flags;
	struct linux_resource *parent, *sibling, *child;
	struct resource *r;
	int rid;
	device_t bsddev;
};



#define devm_request_region(dev,start,n,name) \
	__devm_request_region(dev, &ioport_resource, (start), (n), (name))
#define devm_request_mem_region(dev,start,n,name) \
	__devm_request_region(dev, &iomem_resource, (start), (n), (name))

extern struct linux_resource * __devm_request_region(struct device *dev,
				struct linux_resource *parent, resource_size_t start,
				resource_size_t n, const char *name);
#endif

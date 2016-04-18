#ifndef _LINUX_IOPORT_H
#define _LINUX_IOPORT_H




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



#endif

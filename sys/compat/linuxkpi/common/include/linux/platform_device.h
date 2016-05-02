#ifndef _LINUX__PLATFORM_DEVICE_H_
#define _LINUX__PLATFORM_DEVICE_H_

#include <linux/device.h>
#include <linux/mod_devicetable.h>

#define PLATFORM_DEVID_NONE	(-1)
#define PLATFORM_DEVID_AUTO	(-2)
struct mfd_cell;
struct property_set;
struct platform_device_id;


struct platform_device {
	const char	*name;
	int		id;
	bool		id_auto;
	struct device	dev;
	u32		num_resources;
	struct linux_resource	*resource;

	const struct platform_device_id	*id_entry;
	char *driver_override; /* Driver name to force a match */
#if 0
	
	/* MFD cell pointer */
	struct mfd_cell *mfd_cell;

	/* arch specific additions */
	struct pdev_archdata	archdata;
#endif
};

struct platform_device_info {
	struct device *parent;
	struct fwnode_handle *fwnode;
	
	const char *name;
	int id;

	const struct linux_resource *res;
	unsigned int num_res;

	const void *data;
	size_t size_data;
	u64 dma_mask;

	const struct property_set *pset;
};

extern struct platform_device *platform_device_register_full(const struct platform_device_info *pdevinfo);
extern void platform_device_del(struct platform_device *pdev);
extern int platform_device_add(struct platform_device *pdev);

static inline void
platform_device_put(struct platform_device *pdev)
{
	if (pdev)
		put_device(&pdev->dev);
}
static inline int
platform_device_register(struct platform_device *pdev)
{
	device_initialize(&pdev->dev);
	return (platform_device_add(pdev));
}


static inline void
platform_device_unregister(struct platform_device *pdev)
{
	platform_device_del(pdev);
	platform_device_put(pdev);
}
	
static inline struct platform_device *
platform_device_register_simple(const char *name, int id,
				const struct linux_resource *res, unsigned int num)
{
	const struct platform_device_info pdevinfo = {
		.parent = NULL,
		.name = name,
		.id = id,
		.res = res,
		.num_res = num,
		.data = NULL,
		.size_data = 0,
		.dma_mask = 0,
	};	

	return (platform_device_register_full(&pdevinfo));
}

#endif

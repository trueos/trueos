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
	struct resource	*resource;

	const struct platform_device_id	*id_entry;
	char *driver_override; /* Driver name to force a match */
#if 0
	
	/* MFD cell pointer */
	struct mfd_cell *mfd_cell;

	/* arch specific additions */
	struct pdev_archdata	archdata;
#endif
};

extern int platform_device_register(struct platform_device *);
extern void platform_device_unregister(struct platform_device *);

struct platform_device *platform_device_register_simple(
		const char *name, int id,
		const struct linux_resource *res, unsigned int num);
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");


#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/idr.h>
#include <linux/acpi.h>
#include <linux/property.h>


struct device platform_bus = {
	.init_name	= "platform",
};
static DEFINE_IDA(platform_devid_ida);

struct bus_type platform_bus_type = {
	.name		= "platform",
#ifdef __linux__
	.dev_groups	= platform_dev_groups,
	.match		= platform_match,
	.uevent		= platform_uevent,
	.pm		= &platform_dev_pm_ops,
#endif	
};

int
platform_device_add(struct platform_device *pdev)
{
	int i, rc;
	struct linux_resource *rp, *r;

	if (pdev == NULL)
		return (-EINVAL);

	if (!pdev->dev.parent)
		pdev->dev.parent = &platform_bus;

	pdev->dev.bus = &platform_bus_type;

	switch (pdev->id) {
	default:
		dev_set_name(&pdev->dev, "%s.%d", pdev->name,  pdev->id);
		break;
	case PLATFORM_DEVID_NONE:
		dev_set_name(&pdev->dev, "%s", pdev->name);
		break;
	case PLATFORM_DEVID_AUTO:
		/*
		 * Automatically allocated device ID. We mark it as such so
		 * that we remember it must be freed, and we append a suffix
		 * to avoid namespace collision with explicit IDs.
		 */
		rc = ida_simple_get(&platform_devid_ida, 0, 0, GFP_KERNEL);
		if (rc < 0)
			goto err_2;
		pdev->id = rc;
		pdev->id_auto = true;
		dev_set_name(&pdev->dev, "%s.%d.auto", pdev->name, pdev->id);
		break;
	}

	for (i = 0; i < pdev->num_resources; i++) {
		r = &pdev->resource[i];

		if (r->name == NULL)
			r->name = dev_name(&pdev->dev);

		if (r->parent == NULL) {
			if (resource_type(r) == IORESOURCE_MEM)
				rp = &iomem_resource;
			else if (resource_type(r) == IORESOURCE_IO)
				rp = &ioport_resource;
		} else
			rp = r->parent;
#ifdef notyet		
		if (rp && insert_resource(rp, r)) {
			rc = -EBUSY;
			goto err_1;
		}
#else
		goto err_1;
#endif		
	}

	if ((rc = device_add(&pdev->dev)) == 0)
		return (rc);
 err_1:
	if (pdev->id_auto) {
		ida_simple_remove(&platform_devid_ida, pdev->id);
		pdev->id = PLATFORM_DEVID_AUTO;
	}
#ifdef notyet
	for (int j = 0; j < i; j++) {
		r = &pdev->resource[j];
		if (r->parent)
			release_resource(r);
	}
#endif
 err_2:
	return (rc);
}

struct platform_device *
platform_device_register_full(const struct platform_device_info *pdevinfo)
{
	panic("XXX unimplemented!!!");
	return (NULL);
}

void
platform_device_del(struct platform_device *pdev)
{

}


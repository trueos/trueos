#ifndef _LINUX_ACPI_H_
#define _LINUX_ACPI_H_


#include <linux/errno.h>
#include <linux/ioport.h>	/* for struct resource */
#include <linux/resource_ext.h>
#include <linux/device.h>
#include <linux/property.h>
#include <linux/notifier.h>

#include <linux/list.h>
#include <linux/mod_devicetable.h>



#define CONFIG_ACPI
#include <acpi/acpi.h>
#include <acpi/acpi_bus.h>


static inline acpi_handle acpi_device_handle(struct acpi_device *adev)
{
	return adev ? adev->handle : NULL;
}

#define ACPI_COMPANION(dev)		to_acpi_device_node((dev)->fwnode)
#define ACPI_HANDLE_GET(dev)	acpi_device_handle(ACPI_COMPANION(dev))

struct acpi_device;

static inline long acpi_is_video_device(acpi_handle handle){
	panic("IMPLEMENT ME");
}

#endif

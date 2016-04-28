#ifndef _LINUX_ACPI_H_
#define _LINUX_ACPI_H_


#include <linux/errno.h>
#include <linux/ioport.h>	/* for struct resource */
#include <linux/resource_ext.h>
#include <linux/device.h>
#include <linux/property.h>

#include <acpi/acpi.h>
#include <sys/systm.h>
#include <acpi/acpi.h>
#include <acpi/acpi_bus.h>

#define ACPI_COMPANION(dev)	to_acpi_device_node()
#define ACPI_HANDLE(dev)	acpi_device_handle(ACPI_COMPANION(dev))

struct acpi_device;

static inline acpi_handle acpi_device_handle(struct acpi_device *adev)
{
	panic("IMPLEMENT ME");
        return NULL;
}

static inline long acpi_is_video_device(acpi_handle handle){
	panic("IMPLEMENT ME");
}

#endif

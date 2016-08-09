#ifndef _LINUX_ACPI_H_
#define _LINUX_ACPI_H_


#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/resource_ext.h>
#include <linux/device.h>
#include <linux/property.h>
#include <linux/notifier.h>

#include <linux/list.h>
#include <linux/mod_devicetable.h>



#define CONFIG_ACPI
#include <acpi/acpi.h>
#include <acpi/acpi_bus.h>
#include <acpi/acpi_drivers.h>


static inline acpi_handle acpi_device_handle(struct acpi_device *adev)
{
	return adev ? adev->handle : NULL;
}

#define ACPI_COMPANION(dev)		to_acpi_device_node((dev)->fwnode)
#define ACPI_HANDLE_GET(dev)	acpi_device_handle(ACPI_COMPANION(dev))
#define ACPI_HANDLE(dev)	acpi_device_handle(ACPI_COMPANION(dev))

struct acpi_device;


#define ACPI_VIDEO_OUTPUT_SWITCHING			0x0001
#define ACPI_VIDEO_DEVICE_POSTING			0x0002
#define ACPI_VIDEO_ROM_AVAILABLE			0x0004
#define ACPI_VIDEO_BACKLIGHT				0x0008
#define ACPI_VIDEO_BACKLIGHT_FORCE_VENDOR		0x0010
#define ACPI_VIDEO_BACKLIGHT_FORCE_VIDEO		0x0020
#define ACPI_VIDEO_OUTPUT_SWITCHING_FORCE_VENDOR	0x0040
#define ACPI_VIDEO_OUTPUT_SWITCHING_FORCE_VIDEO		0x0080
#define ACPI_VIDEO_BACKLIGHT_DMI_VENDOR			0x0100
#define ACPI_VIDEO_BACKLIGHT_DMI_VIDEO			0x0200
#define ACPI_VIDEO_OUTPUT_SWITCHING_DMI_VENDOR		0x0400
#define ACPI_VIDEO_OUTPUT_SWITCHING_DMI_VIDEO		0x0800

extern long acpi_is_video_device(acpi_handle handle);

static inline const char *
acpi_dev_name(struct acpi_device *adev)
{
	return (dev_name(&adev->dev));
}

void acpi_scan_drop_device(acpi_handle handle, void *context);


int acpi_get_device_data(acpi_handle handle, struct acpi_device **device,
			 void (*callback)(void *));

struct pci_dev *acpi_get_pci_dev(acpi_handle handle);

static inline int
acpi_bus_get_device(acpi_handle handle, struct acpi_device **device)
{
	return acpi_get_device_data(handle, device, NULL);
}

#endif

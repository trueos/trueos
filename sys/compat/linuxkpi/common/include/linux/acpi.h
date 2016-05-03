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


static inline acpi_handle acpi_device_handle(struct acpi_device *adev)
{
	return adev ? adev->handle : NULL;
}

#define ACPI_COMPANION(dev)		to_acpi_device_node((dev)->fwnode)
#define ACPI_HANDLE_GET(dev)	acpi_device_handle(ACPI_COMPANION(dev))
#define ACPI_HANDLE(dev)	acpi_device_handle(ACPI_COMPANION(dev))

struct acpi_device;

static inline long
acpi_is_video_device(acpi_handle handle)
{
	UNIMPLEMENTED();
	return (0);
}


static inline const char *
acpi_dev_name(struct acpi_device *adev)
{
	return (dev_name(&adev->dev));
}
static inline void
acpi_scan_drop_device(acpi_handle handle, void *context)
{
	UNIMPLEMENTED();
}

static inline int
acpi_get_device_data(acpi_handle handle, struct acpi_device **device,
		     void (*callback)(void *))
{
	acpi_status status;

	if (!device)
		return (-EINVAL);

	status = acpi_get_data_full(handle, acpi_scan_drop_device,
				    (void **)device, callback);
	if (ACPI_FAILURE(status) || !*device) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "No context for object [%p]\n",
				  handle));
		return (-ENODEV);
	}
	return (0);
}

static inline int
acpi_bus_get_device(acpi_handle handle, struct acpi_device **device)
{
	return (acpi_get_device_data(handle, device, NULL));
}
#endif

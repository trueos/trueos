#ifndef _LINUX_ACPI_BUS_H_
#define _LINUX_ACPI_BUS_H_

#include <asm/types.h>
#include <linux/list.h>

typedef char acpi_device_class[20];

struct acpi_bus_event {
	acpi_device_class device_class;
	u32 type;
};

struct acpi_device {
	acpi_handle handle;
	struct list_head children;
	struct list_head node;
};

static inline int acpi_bus_get_device(acpi_handle handle, struct acpi_device **device){
	panic("IMPLEMENT ME");
}

static inline acpi_status
acpi_evaluate_integer(acpi_handle handle,
                      acpi_string pathname,
                      struct acpi_object_list *arguments, unsigned long long *data){
	panic("IMPLEMENT ME");
}

#endif

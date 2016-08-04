#ifndef _LINUX_POWER_SUPPLY_H_
#define _LINUX_POWER_SUPPLY_H_

#include <linux/acpi.h>

static inline int power_supply_is_system_supplied(void) { return -ENOSYS; }

#endif

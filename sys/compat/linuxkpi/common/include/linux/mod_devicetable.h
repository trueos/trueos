#ifndef LINUX_MOD_DEVICETABLE_H
#define LINUX_MOD_DEVICETABLE_H

#define I2C_NAME_SIZE	20
#define I2C_MODULE_PREFIX "i2c:"

struct i2c_device_id {
	char name[I2C_NAME_SIZE];
	uintptr_t driver_data;	/* Data private to the driver */
};
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/kobj.h>
#include <sys/bus.h>
#include <dev/iicbus/iic.h>
#include "iicbus_if.h"
#include <dev/iicbus/iiconf.h>


#include <sys/param.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/sglist.h>
#include <sys/stat.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/filio.h>
#include <sys/rwlock.h>
#include <sys/selinfo.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <sys/queue.h>
#include <sys/signalvar.h>
#include <sys/pciio.h>
#include <sys/poll.h>
#include <sys/sbuf.h>
#include <sys/taskqueue.h>
#include <sys/tree.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_param.h>
#include <vm/vm_phys.h>
#include <machine/bus.h>
#include <machine/resource.h>
#if defined(__i386__) || defined(__amd64__)
#include <machine/specialreg.h>
#endif
#include <machine/sysarch.h>
#include <sys/endian.h>
#include <sys/mman.h>
#include <sys/rman.h>
#include <sys/memrange.h>
#include <dev/agp/agpvar.h>
#include <sys/agpio.h>
#include <sys/mutex.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <sys/selinfo.h>
#include <sys/bus.h>

#include <linux/idr.h>


#include <linux/device.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>


static struct aux_data {
	bool running;
	u16 address;
	void *priv;
	int (*aux_ch)(device_t adapter, int mode, uint8_t write_byte,
	    uint8_t *read_byte);
	device_t port;
};

static int
aux_transaction(device_t idev, int mode, uint8_t write_byte, uint8_t *read_byte)
{
	struct aux_data *aux_data;
	int ret;

	aux_data = device_get_softc(idev);
	ret = (*aux_data->aux_ch)(idev, mode, write_byte, read_byte);
	if (ret < 0)
		return (ret);
	return (0);
}

/*
 * I2C over AUX CH
 */

/*
 * Send the address. If the I2C link is running, this 'restarts'
 * the connection with the new address, this is used for doing
 * a write followed by a read (as needed for DDC)
 */
static int
aux_address(device_t idev, u16 address, bool reading)
{
	struct aux_data *aux_data;
	int mode, ret;

	aux_data = device_get_softc(idev);
	mode = MODE_I2C_START;
	if (reading)
		mode |= MODE_I2C_READ;
	else
		mode |= MODE_I2C_WRITE;
	aux_data->address = address;
	aux_data->running = true;
	ret = iic_dp_aux_transaction(idev, mode, 0, NULL);
	return (ret);
}

/*
 * Stop the I2C transaction. This closes out the link, sending
 * a bare address packet with the MOT bit turned off
 */
static void
aux_stop(device_t idev, bool reading)
{
	struct aux_data *aux_data;
	int mode;

	aux_data = device_get_softc(idev);
	mode = MODE_I2C_STOP;
	if (reading)
		mode |= MODE_I2C_READ;
	else
		mode |= MODE_I2C_WRITE;
	if (aux_data->running) {
		(void)iic_dp_aux_transaction(idev, mode, 0, NULL);
		aux_data->running = false;
	}
}

/*
 * Write a single byte to the current I2C address, the
 * the I2C link must be running or this returns -EIO
 */
static int
aux_put_byte(device_t idev, u8 byte)
{
	struct aux_data *aux_data;
	int ret;

	aux_data = device_get_softc(idev);

	if (!aux_data->running)
		return (-EIO);

	ret = iic_dp_aux_transaction(idev, MODE_I2C_WRITE, byte, NULL);
	return (ret);
}

/*
 * Read a single byte from the current I2C address, the
 * I2C link must be running or this returns -EIO
 */
static int
aux_get_byte(device_t idev, u8 *byte_ret)
{
	struct aux_data *aux_data;
	int ret;

	aux_data = device_get_softc(idev);

	if (!aux_data->running)
		return (-EIO);

	ret = iic_dp_aux_transaction(idev, MODE_I2C_READ, 0, byte_ret);
	return (ret);
}

static int
aux_xfer(device_t idev, struct iic_msg *msgs, uint32_t num)
{
	u8 *buf;
	int b, m, ret;
	u16 len;
	bool reading;

	ret = 0;
	reading = false;

	for (m = 0; m < num; m++) {
		len = msgs[m].len;
		buf = msgs[m].buf;
		reading = (msgs[m].flags & IIC_M_RD) != 0;
		ret = iic_dp_aux_address(idev, msgs[m].slave >> 1, reading);
		if (ret < 0)
			break;
		if (reading) {
			for (b = 0; b < len; b++) {
				ret = iic_dp_aux_get_byte(idev, &buf[b]);
				if (ret != 0)
					break;
			}
		} else {
			for (b = 0; b < len; b++) {
				ret = iic_dp_aux_put_byte(idev, buf[b]);
				if (ret < 0)
					break;
			}
		}
		if (ret != 0)
			break;
	}
	iic_dp_aux_stop(idev, reading);
	DRM_DEBUG_KMS("dp_aux_xfer return %d\n", ret);
	return (-ret);
}

static void
aux_reset_bus(device_t idev)
{

	(void)aux_address(idev, 0, false);
	(void)aux_stop(idev, false);
}

static int
aux_reset(device_t idev, u_char speed, u_char addr, u_char *oldaddr)
{

	aux_reset_bus(idev);
	return (0);
}

static int
aux_prepare_bus(device_t idev)
{

	/* adapter->retries = 3; */
	reset_bus(idev);
	return (0);
}

static int
aux_probe(device_t idev)
{

	return (BUS_PROBE_DEFAULT);
}

static int
aux_attach(device_t idev)
{
	struct aux_data *aux_data;

	aux_data = device_get_softc(idev);
	aux_data->port = device_add_child(idev, "iicbus", -1);
	if (aux_data->port == NULL)
		return (ENXIO);
	device_quiet(aux_data->port);
	bus_generic_attach(idev);
	return (0);
}

static int
aux_add_bus(device_t dev, const char *name,
    int (*ch)(device_t idev, int mode, uint8_t write_byte, uint8_t *read_byte),
    void *priv, device_t *bus, device_t *adapter)
{
	device_t ibus;
	struct aux_data *data;
	int idx, error;
	static unsigned int dp_bus_counter;

	mtx_lock(&Giant);

	idx = atomic_fetchadd_int(&dp_bus_counter, 1);
	ibus = device_add_child(dev, "i2c_aux", idx);
	if (ibus == NULL) {
		mtx_unlock(&Giant);
		DRM_ERROR("drm_iic_dp_aux bus %d creation error\n", idx);
		return (-ENXIO);
	}
	device_quiet(ibus);
	error = device_probe_and_attach(ibus);
	if (error != 0) {
		device_delete_child(dev, ibus);
		mtx_unlock(&Giant);
		DRM_ERROR("drm_iic_dp_aux bus %d attach failed, %d\n",
		    idx, error);
		return (-error);
	}
	data = device_get_softc(ibus);
	data->running = false;
	data->address = 0;
	data->aux_ch = ch;
	data->priv = priv;
	error = aux_prepare_bus(ibus);
	if (error == 0) {
		*bus = ibus;
		*adapter = data->port;
	}
	mtx_unlock(&Giant);
	return (-error);
}

static device_method_t i2c_aux_methods[] = {
	DEVMETHOD(device_probe,		aux_probe),
	DEVMETHOD(device_attach,	aux_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(iicbus_reset,		aux_reset),
	DEVMETHOD(iicbus_transfer,	aux_xfer),
	DEVMETHOD_END
};
static driver_t i2c_aux_driver = {
	"aux",
	i2c_aux_methods,
	sizeof(struct aux_data)
};
static devclass_t i2c_aux_devclass;
DRIVER_MODULE_ORDERED(i2c_aux, drmn, i2c_aux_driver,
    i2c_aux_devclass, 0, 0, SI_ORDER_SECOND);



static int
i2c_aux_ch(device_t dev, int mode, u8 write_byte, u8 *read_byte)
{
	struct i2c_aux_data *algo_data = device_get_softc(dev);
	struct i2c_adapter *adap = algo_data->priv;
	struct i2c_algorithm *algo = adap->algo;
	struct i2c_msg msg;
	u8 reply[2];
	unsigned retry;
	int ret;
	u8 ack;

	msg.addr = algo_data->address;

	/* Set up the command byte */
	if (mode & MODE_I2C_READ)
		msg.flags = I2C_SMBUS_READ << 4;
	else
		msg.flags = I2C_SMBUS_WRITE << 4;

	if (!(mode & MODE_I2C_STOP))
		msg.flags |= AUX_I2C_MOT << 4;

	switch (mode) {
	case MODE_I2C_WRITE:
		msg.len = 1;
		msg.buf = &write_byte;
		break;
	case MODE_I2C_READ:
		msg.len = 1;
		msg.buf = read_byte;
		break;
	default:
		msg.len = 0;
		break;
	}

	for (retry = 0; retry < 4; retry++) {
		ret = algo.master_xfer(adap, &msg, 1);

		if (ret == 1)
			return (0);
		if (ret == -EBUSY)
			continue;
		else if (ret < 0) {
			DRM_DEBUG_KMS("aux_ch failed %d\n", ret);
			return (ret);
		}

	}

	DRM_DEBUG_KMS("aux i2c too many retries, giving up\n");
	return -EREMOTEIO;
}

static int
i2c_register_adapter(struct i2c_adapter *adap)
{
	int res;
	device_t bus;
	device_t aux_adapter;

	if (__predict_false(adap->name[0] == '\0'))
		return -EINVAL;
	if (__predict_false(!adap->algo))
		return -EINVAL;

	mutex_init(&adap->bus_lock);
	mutex_init(&adap->userspace_clients_lock);
	INIT_LIST_HEAD(&adap->userspace_clients);

	if (adap->timeout == 0)
		adap->timeout = hz;

	dev_set_name(&adap->dev, "i2c-%d", adap->nr);
	adap->dev.bus = &i2c_bus_type;
	adap->dev.type = &i2c_adapter_type;
	res = device_register(&adap->dev);
	if (res)
		goto err;

	aux_add_bus(adap->dev.bsddev, i2c_aux_ch, adap, &bus, &aux_adapter);
	adap->bsd_bus = bus;
	adap->bsd_adapter = aux_adapter;
	return 0;
err:
	mtx_lock(&Giant);
	idr_remove(&i2c_adapter_idr, adap->nr);
	mtx_unlock(&Giant);
	return res;
}

static int
bit_xfer(struct i2c_adapter *i2c_adap,
		    struct i2c_msg msgs[], int num) 
{
	panic("IMPLEMENT ME!!!");
	return (EINVAL);
}

static u32
bit_func(struct i2c_adapter *adap)
{
	return (I2C_FUNC_I2C | I2C_FUNC_NOSTART | I2C_FUNC_SMBUS_EMUL |
		I2C_FUNC_SMBUS_READ_BLOCK_DATA |
		I2C_FUNC_SMBUS_BLOCK_PROC_CALL |
		I2C_FUNC_10BIT_ADDR | I2C_FUNC_PROTOCOL_MANGLING);
}


const struct i2c_algorithm i2c_bit_algo = {
	.master_xfer	= bit_xfer,
	.functionality	= bit_func,
};

int
i2c_add_adapter(struct i2c_adapter *adapter)
{
	int id;

	mtx_lock(&Giant);
	id = idr_alloc(i2c_adapter_idr, __i2c_first_dynamic_bus_num, 0, GFP_KERNEL);
	mtx_unlock(&Giant);

	if (id < 0)
		return id;

	adapter->nr = id;
	return (i2c_register_adapter(adapter));
}


int
i2c_del_adapter(struct i2c_adapter *adap)
{
	int found;
	struct i2c_client *client, *next;

	mtx_lock(&Giant);
	found = idr_find(&i2c_adapter_idr, adap->nr);
	mtx_unlock(&Giant);
	if (found != adap)
		return (-EINVAL);

	mtx_lock(&Giant);
	/* which one do I detach? */
	device_detach(adap->bsd_adapter);
	mtx_unlock(&Giant);

	/* Remove devices instantiated from sysfs */
	mutex_lock_nested(&adap->userspace_clients_lock,
			  i2c_adapter_depth(adap));
	list_for_each_entry_safe(client, next, &adap->userspace_clients,
				 detected) {
		dev_dbg(&adap->dev, "Removing %s at 0x%x\n", client->name,
			client->addr);
		list_del(&client->detected);
		i2c_unregister_device(client);
	}
	mutex_unlock(&adap->userspace_clients_lock);

	device_unregister(&adap->dev);

	mtx_lock(&Giant);
	idr_remove(&i2c_adapter_idr, adap->nr);
	mutex_unlock(&Giant);
}

static uint16_t
i2c2iic_flags(uint16_t flags)
{

	return (0);
}


static uint16_t
iic2i2c_flags(uint16_t flags)
{
	return (0);
}

int
i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	int i, rc;

	for (i = 0; i < num; i++) {
		msgs[i].flags = i2ciic_flags(msgs[i].flags);
	}

	if ((rc = iicbus_transfer(adap->bsd_bus, msgs, num)))
		return (-rc);

	for (i = 0; i < num; i++) {
		msgs[i].flags = iici2c_flags(msgs[i].flags);
	}
}

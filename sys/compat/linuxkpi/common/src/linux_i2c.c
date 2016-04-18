#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/bus.h>

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

static DEFINE_IDR(i2c_adapter_idr);

#define I2C_ERR printf

static int
i2c_register_adapter(struct i2c_adapter *adap)
{
	int rc;

	if (__predict_false(adap->name[0] == '\0'))
		return (-EINVAL);
	if (__predict_false(!adap->algo))
		return (-EINVAL);

	mutex_init(&adap->bus_lock);

	if (adap->timeout == 0)
		adap->timeout = hz;

	dev_set_name(&adap->dev, "i2c-%d", adap->nr);
#ifdef notyet
	adap->dev.bus = &i2c_bus_type;
	adap->dev.type = &i2c_adapter_type;
#endif
	rc = device_register(&adap->dev);
	if (rc)
		goto err;
	return (0);
err:
	mtx_lock(&Giant);
	idr_remove(&i2c_adapter_idr, adap->nr);
	mtx_unlock(&Giant);
	return (rc);
}


#define setsda(adap, val)	adap->setsda(adap->data, val)
#define setscl(adap, val)	adap->setscl(adap->data, val)
#define getsda(adap)		adap->getsda(adap->data)
#define getscl(adap)		adap->getscl(adap->data)

#define I2C_SET(adap, ctrl, data) do {		\
	setscl(adap, ctrl);			\
	setsda(adap, data);			\
	} while (0)

static inline void
sdalo(struct i2c_algo_bit_data *adap)
{
	setsda(adap, 0);
	DELAY((adap->udelay + 1) / 2);
}

static inline void
sdahi(struct i2c_algo_bit_data *adap)
{
	setsda(adap, 1);
	DELAY((adap->udelay + 1) / 2);
}

static inline void
scllo(struct i2c_algo_bit_data *adap)
{
	setscl(adap, 0);
	DELAY(adap->udelay / 2);
}

static int
sclhi(struct i2c_algo_bit_data *adap)
{
	unsigned long orig_ticks;


	setscl(adap, 1);
	if (adap->getscl == NULL)
		goto end;

	orig_ticks = ticks;
	while (!getscl(adap)) {
		if (ticks  > orig_ticks + adap->timeout) {

			if (getscl(adap))
			    break;
			return (-ETIMEDOUT);
		}
		cpu_spinwait();
	}


end:
	DELAY(adap->udelay);
	return (0);
}

static void
i2c_one(struct i2c_algo_bit_data *adap)
{
	I2C_SET(adap, 0, 1);
	I2C_SET(adap, 1, 1);
	I2C_SET(adap, 0, 1);
}

static void
i2c_zero(struct i2c_algo_bit_data *adap)
{
	I2C_SET(adap, 0, 0);
	I2C_SET(adap, 1, 0);
	I2C_SET(adap, 0, 0);
}

static void
i2c_txn_start(struct i2c_algo_bit_data *adap)
{
	setsda(adap, 0);
	DELAY(adap->udelay);
	scllo(adap);
}

static void
i2c_txn_restart(struct i2c_algo_bit_data *adap)
{
	/* assert: scl is low */
	sdahi(adap);
	sclhi(adap);
	setsda(adap, 0);
	DELAY(adap->udelay);
	scllo(adap);
}

static void
i2c_txn_stop(struct i2c_algo_bit_data *adap)
{
	sdalo(adap);
	sclhi(adap);
	setsda(adap, 1);
	DELAY(adap->udelay);
}

static int
i2c_sendbyte(struct i2c_algo_bit_data *adap, unsigned char data)
{
	int i, ack;

	for (i=7; i>=0; i--) {
		if (data&(1<<i)) {
			i2c_one(adap);
		} else {
			i2c_zero(adap);
		}
	}

	ack = (getsda(adap) == 0);
	scllo(adap);
	return ack;
}

static u_char
i2c_readbyte(struct i2c_algo_bit_data *adap)
{
	int i;
	unsigned char data;

	data = 0;
	sdahi(adap);
	for (i = 7; i >=0; i--) {
		if (sclhi(adap) < 0)
			return (-ETIMEDOUT);
		if (getsda(adap))
			data |= (1 << i);
		setscl(adap, 0);
		DELAY(i == 0 ? adap->udelay / 2 : adap->udelay);
	}
	return (data);
}


static int
test_addr(struct i2c_algo_bit_data *adap, unsigned char addr, int retries)
{
	int i, ret = 0;

	for (i = 0; i <= retries; i++) {
		ret = i2c_sendbyte(adap, addr);
		if (ret == 1 || i == retries)
			break;
		i2c_txn_stop(adap);
		DELAY(adap->udelay);
		i2c_txn_start(adap);
	}
	return (ret);
}


static int
i2c_address(struct i2c_adapter *i2c_adap, struct i2c_msg *msg)
{
	unsigned short flags = msg->flags;
	unsigned short nak_ok = msg->flags & I2C_M_IGNORE_NAK;
	struct i2c_algo_bit_data *adap = i2c_adap->algo_data;

	unsigned char addr;
	int rc, retries;

	retries = nak_ok ? 0 : i2c_adap->retries;

	if (flags & I2C_M_TEN) {
		panic("10-bit i2c addresses not currently supported");
	} else {
		addr = msg->addr << 1;
		if (flags & I2C_M_RD)
			addr |= 1;
		if (flags & I2C_M_REV_DIR_ADDR)
			addr ^= 1;
		rc = test_addr(adap, addr, retries);
		if (rc != 1 && !nak_ok)
			return (-ENXIO);
	}
	return (0);
}

static int
i2c_send_acknack(struct i2c_adapter *i2c_adap, int do_ack)
{
	struct i2c_algo_bit_data *adap = i2c_adap->algo_data;

	if (do_ack)
		setsda(adap, 0);
	DELAY((adap->udelay + 1) / 2);
	if (sclhi(adap) < 0)
		return (-ETIMEDOUT);
	scllo(adap);
	return (0);
}

static int
i2c_get_bytes(struct i2c_adapter *i2c_adap, struct i2c_msg *pmsg)
{
	int i, rc, count, len, flags;
	unsigned char *pbuf;

	flags = pmsg->flags;
	pbuf = pmsg->buf;
	count = 0;
	len = pmsg->len;
	for (i = 0; i < len; i++) {
		rc = i2c_readbyte(i2c_adap->algo_data);
		if (rc < 0)
			break;
		*pbuf = rc;
		pbuf++;
		count++;
		if (count == 1 && (flags & I2C_M_RECV_LEN)) {
			if (rc > I2C_SMBUS_BLOCK_MAX) {
				if (!(flags & I2C_M_NO_RD_ACK))
					i2c_send_acknack(i2c_adap, 0);
				return (-EPROTO);
			}
			len += rc;
			pmsg->len += rc;
		}
		if ((flags & I2C_M_NO_RD_ACK) == 0) {
			rc = i2c_send_acknack(i2c_adap, len - count);
			if (rc  < 0)
				return (rc);
		}
	}

	return (count);
}

static int
i2c_put_bytes(struct i2c_adapter *i2c_adap, struct i2c_msg *pmsg)
{
	int i, rc, nak_ok;

	nak_ok = 0;
	for (i = 0; i < pmsg->len; i++) {
		rc = i2c_sendbyte(i2c_adap->algo_data, *(pmsg->buf + i));
		if (rc == 0 && !nak_ok)
			return (-EIO);
		else if (rc < 0)
			return (rc);
	}

	return (pmsg->len);
}

static int
bit_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[], int num)
{
	struct i2c_msg *pmsg;
	struct i2c_algo_bit_data *adap = i2c_adap->algo_data;
	int i, rc;
	unsigned short nak_ok;


	rc = 0;
	if (adap->pre_xfer) {
		if ((rc = adap->pre_xfer(i2c_adap)) < 0)
			return (rc);
	}

	i2c_txn_start(adap);
	for (i = 0; i < num; i++) {
		pmsg = &msgs[i];
		nak_ok = pmsg->flags & I2C_M_IGNORE_NAK;
		if (!(pmsg->flags & I2C_M_NOSTART)) {
			if (i)
				i2c_txn_restart(adap);
			if ((rc = i2c_address(i2c_adap, pmsg)) && !nak_ok)
				goto err;
		}
		if (pmsg->flags & I2C_M_RD)
			rc = i2c_get_bytes(i2c_adap, pmsg);
		else
			rc = i2c_put_bytes(i2c_adap, pmsg);
		if (rc < pmsg->len) {
			if (rc >= 0)
				rc = -EIO;
			goto err;
		}
	}
	rc = i;
err:
	i2c_txn_stop(adap);
	if (adap->post_xfer)
		adap->post_xfer(i2c_adap);
	return (rc);
}

static uint32_t
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
	static int __i2c_first_dynamic_bus_num;

	mtx_lock(&Giant);
	id = idr_alloc(&i2c_adapter_idr, adapter, __i2c_first_dynamic_bus_num, 0, GFP_KERNEL);
	mtx_unlock(&Giant);

	if (id < 0)
		return id;
	__i2c_first_dynamic_bus_num++;
	adapter->nr = id;
	return (i2c_register_adapter(adapter));
}

int
i2c_del_adapter(struct i2c_adapter *adap)
{
	struct i2c_adapter *found;

	mtx_lock(&Giant);
	found = idr_find(&i2c_adapter_idr, adap->nr);
	mtx_unlock(&Giant);
	if (found != adap)
		return (-EINVAL);

	device_unregister(&adap->dev);

	mtx_lock(&Giant);
	idr_remove(&i2c_adapter_idr, adap->nr);
	mtx_unlock(&Giant);
	return (0);
}

static int
__i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	uint64_t orig_ticks;
	int rc, iter;

	orig_ticks = ticks;
	for (rc = iter = 0; iter <= adap->retries; iter++) {
		rc = adap->algo->master_xfer(adap, msgs, num);
		if (rc != -EAGAIN)
			break;
		if (ticks > orig_ticks + adap->timeout)
			break;
	}

	return (rc);
}

int
i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	int rc;

	if (adap->algo->master_xfer == NULL)
		return (-EOPNOTSUPP);

	mtx_lock(&Giant);
	rc = __i2c_transfer(adap, msgs, num);
	mtx_unlock(&Giant);
	return (rc);
}

/*-
 * Copyright (c) 2015-2016 Mellanox Technologies, Ltd.
 * Copyright (c) 2016 Matthew Macy
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filio.h>
#include <sys/rwlock.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/stdarg.h>

#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/file.h>
#include <linux/sysfs.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/pci.h>
#include <linux/compat.h>

/* assumes !e820 */
unsigned long pci_mem_start;


const char *pci_power_names[] = {
	"error", "D0", "D1", "D2", "D3hot", "D3cold", "unknown",
};

static device_probe_t linux_pci_probe;
static device_attach_t linux_pci_attach;
static device_detach_t linux_pci_detach;
static device_suspend_t linux_pci_suspend;
static device_resume_t linux_pci_resume;
static device_shutdown_t linux_pci_shutdown;

static device_method_t pci_methods[] = {
	DEVMETHOD(device_probe, linux_pci_probe),
	DEVMETHOD(device_attach, linux_pci_attach),
	DEVMETHOD(device_detach, linux_pci_detach),
	DEVMETHOD(device_suspend, linux_pci_suspend),
	DEVMETHOD(device_resume, linux_pci_resume),
	DEVMETHOD(device_shutdown, linux_pci_shutdown),
	DEVMETHOD_END
};

static struct pci_driver *
linux_pci_find(device_t dev, const struct pci_device_id **idp)
{
	const struct pci_device_id *id;
	struct pci_driver *pdrv;
	uint16_t vendor;
	uint16_t device;

	vendor = pci_get_vendor(dev);
	device = pci_get_device(dev);

	spin_lock(&pci_lock);
	list_for_each_entry(pdrv, &pci_drivers, links) {
		for (id = pdrv->id_table; id->vendor != 0; id++) {
			if (vendor == id->vendor && device == id->device) {
				*idp = id;
				spin_unlock(&pci_lock);
				return (pdrv);
			}
		}
	}
	spin_unlock(&pci_lock);
	return (NULL);
}

static int
linux_pci_probe(device_t dev)
{
	const struct pci_device_id *id;
	struct pci_driver *pdrv;

	if ((pdrv = linux_pci_find(dev, &id)) == NULL) {
		printf("linux_pci_find failed!\n");
		return (ENXIO);
	}
	if (device_get_driver(dev) != &pdrv->bsd_driver) {
		printf("device_get_driver failed!\n");
		return (ENXIO);
	}
	device_set_desc(dev, pdrv->name);
	return (0);
}

static int
linux_pci_attach(device_t dev)
{
	struct resource_list_entry *rle;
	struct pci_dev *pdev;
	struct pci_driver *pdrv;
	const struct pci_device_id *id;
	struct pci_bus *pbus;
	devclass_t dc;
	device_t ggparent, gparent, parent;
	int error, isroot;

	isroot = error = 0;
	linux_set_current(curthread);
	parent = device_get_parent(dev);
	dc = device_get_devclass(parent);
	if (strcmp(devclass_get_name(dc), "pci") != 0) {
		device_set_ivars(dev, device_get_ivars(parent));
		gparent = device_get_parent(parent);
		ggparent = device_get_parent(gparent);
		if (ggparent != NULL)
			gparent = ggparent;
	} else
		gparent = device_get_parent(parent);

	dc = device_get_devclass(gparent);
	if (strcmp(devclass_get_name(dc), "nexus") == 0)
		isroot = 1;

	pdrv = linux_pci_find(dev, &id);
	pdev = device_get_softc(dev);
	pdev->dev.parent = &linux_root_device;
	pdev->dev.bsddev = dev;
	if (pdev->bus == NULL) {
		pbus = malloc(sizeof(*pbus), M_DEVBUF, M_WAITOK|M_ZERO);
		if (isroot == 0)
			pbus->self = pdev;
		pdev->bus = pbus;
	}

	INIT_LIST_HEAD(&pdev->dev.irqents);
	pdev->bus->number = pci_get_bus(dev);
	pdev->devfn = PCI_DEVFN(pci_get_slot(dev), pci_get_function(dev));
	pdev->device = id->device;
	pdev->vendor = id->vendor;
	pdev->revision = pci_get_revid(dev);
	pdev->class = pci_get_class(dev);
	pdev->dev.dma_mask = &pdev->dma_mask;
	pdev->pdrv = pdrv;
	kobject_init(&pdev->dev.kobj, &linux_dev_ktype);
	kobject_set_name(&pdev->dev.kobj, device_get_nameunit(dev));
	kobject_add(&pdev->dev.kobj, &linux_root_device.kobj,
	    kobject_name(&pdev->dev.kobj));
	rle = linux_pci_get_rle(pdev, SYS_RES_IRQ, 0);
	if (rle != NULL)
		pdev->dev.irq = rle->start;
	else
		pdev->dev.irq = LINUX_IRQ_INVALID;
	pdev->irq = pdev->dev.irq;
	DROP_GIANT();
	spin_lock(&pci_lock);
	list_add(&pdev->links, &pci_devices);
	spin_unlock(&pci_lock);
	error = pdrv->probe(pdev, id);
	PICKUP_GIANT();
	if (error) {
		spin_lock(&pci_lock);
		list_del(&pdev->links);
		spin_unlock(&pci_lock);
		put_device(&pdev->dev);
		device_printf(dev, "linux_pci_attach failed! %d", error);
		error = -error;
	}
	return (error);
}

static int
linux_pci_detach(device_t dev)
{
	struct pci_dev *pdev;

	linux_set_current(curthread);
	pdev = device_get_softc(dev);
	DROP_GIANT();
	pdev->pdrv->remove(pdev);
	PICKUP_GIANT();
	spin_lock(&pci_lock);
	list_del(&pdev->links);
	spin_unlock(&pci_lock);
	put_device(&pdev->dev);

	return (0);
}

static int
linux_pci_suspend(device_t dev)
{
	struct pm_message pm = { };
	struct pci_dev *pdev;
	int err;

	linux_set_current(curthread);
	pdev = device_get_softc(dev);
	if (pdev->pdrv->suspend != NULL)
		err = -pdev->pdrv->suspend(pdev, pm);
	else
		err = 0;
	return (err);
}

static int
linux_pci_resume(device_t dev)
{
	struct pci_dev *pdev;
	int err;

	linux_set_current(curthread);
	pdev = device_get_softc(dev);
	if (pdev->pdrv->resume != NULL)
		err = -pdev->pdrv->resume(pdev);
	else
		err = 0;
	return (err);
}

static int
linux_pci_shutdown(device_t dev)
{
	struct pci_dev *pdev;

	linux_set_current(curthread);
	pdev = device_get_softc(dev);
	if (pdev->pdrv->shutdown != NULL) {
		DROP_GIANT();
		pdev->pdrv->shutdown(pdev);
		PICKUP_GIANT();
	}
	return (0);
}

static int
pci_default_suspend(struct pci_dev *dev, pm_message_t state __unused)
{
        int err = 0;

        if (dev->pdrv->driver.pm->suspend != NULL) {
                err = -dev->pdrv->driver.pm->suspend(&(dev->dev));
		if (err == 0 && dev->pdrv->driver.pm->suspend_late != NULL)
			err = -dev->pdrv->driver.pm->suspend_late(&(dev->dev));
	}

        return (err);
}

static int
pci_default_resume(struct pci_dev *dev)
{
        int err = 0;

	if (dev->pdrv->driver.pm->resume_early != NULL )
		if ((err = -dev->pdrv->driver.pm->resume_early(&(dev->dev)))) {
			printf("resume early failed: %d\n", -err);
			return (err);
		}
        if (dev->pdrv->driver.pm->resume != NULL)
                err = -dev->pdrv->driver.pm->resume(&(dev->dev));
	if (err)
		printf("resume failed: %d\n", -err);
        return (err);
}

int
pci_register_driver(struct pci_driver *pdrv)
{
	devclass_t bus;
	int error = 0;

	if (pdrv->busname != NULL)
		bus = devclass_create(pdrv->busname);
	else
		bus = devclass_find("pci");

	linux_set_current(curthread);
	spin_lock(&pci_lock);
	list_add(&pdrv->links, &pci_drivers);
	spin_unlock(&pci_lock);
	pdrv->bsd_driver.name = pdrv->name;
	pdrv->bsd_driver.methods = pci_methods;
	if (pdrv->suspend == NULL)
		pdrv->suspend = pci_default_suspend;
	if (pdrv->resume == NULL)
		pdrv->resume = pci_default_resume;

	pdrv->bsd_driver.size = sizeof(struct pci_dev);

	mtx_lock(&Giant);
	if (bus != NULL) {
		error = devclass_add_driver(bus, &pdrv->bsd_driver, BUS_PASS_DEFAULT,
		    pdrv->bsdclass);
		if (error)
			printf("devclass_add_driver failed with %d\n", error);
	} else
		error = -ENXIO;
	mtx_unlock(&Giant);

	return (-error);
}

void
pci_unregister_driver(struct pci_driver *pdrv)
{
	devclass_t bus;

	if (pdrv->busname != NULL)
		bus = devclass_create(pdrv->busname);
	else
		bus = devclass_find("pci");

	list_del(&pdrv->links);
	mtx_lock(&Giant);
	if (bus != NULL)
		devclass_delete_driver(bus, &pdrv->bsd_driver);
	mtx_unlock(&Giant);
}

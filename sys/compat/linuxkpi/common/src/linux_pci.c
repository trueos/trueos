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
#include <linux/ioport.h>
#include <linux/compat.h>

#undef resource
#define IO_SPACE_LIMIT 0xffff

/* XXX asumes x86 */
#include <asm/io.h>

/* assumes !e820 */
unsigned long pci_mem_start;

struct linux_resource ioport_resource = {
	.name	= "PCI IO",
	.start	= 0,
	.end	= IO_SPACE_LIMIT,
	.flags	= IORESOURCE_IO,
};


struct linux_resource iomem_resource = {
	.name	= "PCI mem",
	.start	= 0,
	.end	= -1,
	.flags	= IORESOURCE_MEM,
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


struct pci_dev *
linux_bsddev_to_pci_dev(device_t dev)
{
	struct pci_dev *pdev;

	spin_lock(&pci_lock);
	list_for_each_entry(pdev, &pci_devices, links) {
		if (pdev->dev.bsddev == dev)
			break;
	}
	spin_unlock(&pci_lock);

	return (pdev);
}

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
	if (device_get_driver(dev) != &pdrv->driver) {
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
	linux_set_current();
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
	/* XXX how do we check this ? assume true */
	pdev->msix_enabled = 1;
	pdev->msi_enabled = 1;
	pdev->pdrv = pdrv;
	kobject_init(&pdev->dev.kobj, &linux_dev_ktype);
	kobject_set_name(&pdev->dev.kobj, device_get_nameunit(dev));
	kobject_add(&pdev->dev.kobj, &linux_root_device.kobj,
	    kobject_name(&pdev->dev.kobj));
	rle = _pci_get_rle(pdev, SYS_RES_IRQ, 0);
	if (rle)
		pdev->dev.irq = rle->start;
	else
		pdev->dev.irq = 255;
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

	linux_set_current();
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

	linux_set_current();
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

	linux_set_current();
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

	linux_set_current();
	pdev = device_get_softc(dev);
	if (pdev->pdrv->shutdown != NULL) {
		DROP_GIANT();
		pdev->pdrv->shutdown(pdev);
		PICKUP_GIANT();
	}
	return (0);
}

int pci_default_suspend(struct pci_dev *dev,
                        pm_message_t state __unused)
{
        int err;

        if(dev->pdrv->linux_driver.pm->suspend != NULL)
                err = -dev->pdrv->linux_driver.pm->suspend(&(dev->dev));
        else
                err = 0;

        return (err);
}

int pci_default_resume(struct pci_dev *dev)
{
        int err;

        if(dev->pdrv->linux_driver.pm->resume != NULL)
                err = -dev->pdrv->linux_driver.pm->resume(&(dev->dev));
        else
                err = 0;

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

	linux_set_current();
	spin_lock(&pci_lock);
	list_add(&pdrv->links, &pci_drivers);
	spin_unlock(&pci_lock);
	pdrv->driver.name = pdrv->name;
	pdrv->driver.methods = pci_methods;
	pdrv->driver.size = sizeof(struct pci_dev);

	mtx_lock(&Giant);
	if (bus != NULL) {
		error = devclass_add_driver(bus, &pdrv->driver, BUS_PASS_DEFAULT,
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
		devclass_delete_driver(bus, &pdrv->driver);
	mtx_unlock(&Giant);
}

void *
pci_iomap(struct pci_dev *pdev, int bar, unsigned long max)
{
	struct resource *res;
	int rid, len, type;
	void *regs;

	type = 0;
	if (pdev->pcir.r[bar] == NULL) {
		rid = PCIR_BAR(bar);
		type = pci_resource_flags(pdev, bar);
		if ((res = bus_alloc_resource_any(pdev->dev.bsddev, type,
						  &rid, RF_ACTIVE)) == NULL)
			return (NULL);
		pdev->pcir.r[bar] = res;
		pdev->pcir.rid[bar] = rid;
		pdev->pcir.type[bar] = type;
		regs = (void *)rman_get_bushandle(pdev->pcir.r[bar]);
		len = rman_get_end(pdev->pcir.r[bar])  - rman_get_start(pdev->pcir.r[bar]);

		pdev->pcir.map[bar] = regs;

	}
	return (pdev->pcir.map[bar]);
}


void
pci_iounmap(struct pci_dev *pdev, void *regs)
{
	int bar, rid, type;
	struct resource *res;

	res = NULL;
	for (bar = 0; bar <= LINUXKPI_MAX_PCI_RESOURCE; bar++) {
		if (pdev->pcir.map[bar] != regs)
			continue;
		res = pdev->pcir.r[bar];
		rid = pdev->pcir.rid[bar];
		type = pdev->pcir.type[bar];
	}

	if (res == NULL)
		return;

	bus_release_resource(pdev->dev.bsddev, type, rid, res);
}


struct pci_dev *
pci_get_bus_and_slot(unsigned int bus, unsigned int devfn)
{
	device_t dev;
	struct pci_dev *pdev;
	struct pci_bus *pbus;

	dev = pci_find_bsf(bus, devfn >> 16, devfn & 0xffff);
	if (dev == NULL)
		return (NULL);

	pdev = malloc(sizeof(*pdev), M_DEVBUF, M_WAITOK|M_ZERO);
	pdev->devfn = devfn;
	pdev->dev.bsddev = dev;
	pbus = malloc(sizeof(*pbus), M_DEVBUF, M_WAITOK|M_ZERO);
	pbus->self = pdev;
	pdev->bus = pbus;
	return (pdev);
}

void
pci_dev_put(struct pci_dev *pdev)
{
	if (pdev == NULL)
		return;

	MPASS(pdev->bus);
	MPASS(pdev->bus->self == pdev);
	free(pdev->bus, M_DEVBUF);
	free(pdev, M_DEVBUF);
}

resource_size_t
pcibios_align_resource(void *data, const struct linux_resource *res,
		       resource_size_t size __unused, resource_size_t align __unused)
{
	resource_size_t start = res->start;
	/* ignore IO resources */

	return (start);
}

int __must_check
pci_bus_alloc_resource(struct pci_bus *bus,
			struct linux_resource *res, resource_size_t size,
			resource_size_t align, resource_size_t min,
			unsigned int type_mask,
			resource_size_t (*alignf)(void *,
						  const struct linux_resource *,
						  resource_size_t,
						  resource_size_t),
					void *alignf_data)
{
	struct pci_devinfo *dinfo;
	struct resource_list *rl;
	device_t dev;
	struct resource *r;
	struct resource_list_entry *rle;
	int type, flags;

	/* XXX initialize bus */
	dev = bus->self->dev.bsddev;
	dinfo = device_get_ivars(bus->self->dev.bsddev);
	rl = &dinfo->resources;

	/* transform flags to BSD XXX */
	flags = res->flags;
	/*assuming memory not I/O or intr*/
	type = SYS_RES_MEMORY;

	res->bsddev = dev;
	STAILQ_FOREACH(rle, rl, link) {
		if (rle->type != type)
			continue;
		if ((flags ^ rle->flags) & type_mask)
			continue;
		if (rle->start < min)
			continue;
		if (rle->end - rle->start < size)
			continue;
		/* XXX check against PREFETCH */
		res->rid = rle->rid;
		res->start = rle->start;
		res->end = rle->end;
		res->type = type;
		r = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &res->rid, RF_SHAREABLE);
		if (r)
			break;
	}
	if (r == NULL)
		return (-ENOMEM);

	res->r = r;
	return (0);
}

int
release_resource(struct linux_resource *lr)
{
	int rc;

	rc = bus_release_resource(lr->bsddev, lr->type, lr->rid, lr->r);
	lr->bsddev = NULL;
	lr->r = NULL;
	lr->rid = -1;
	lr->type = -1;
	if (rc)
		return (-EINVAL);

	return (0);
}

struct pci_dev *
linux_pci_get_class(unsigned int class, struct pci_dev *from)
{
	device_t dev;
	struct pci_dev *pdev;
	struct pci_bus *pbus;
	int pcic, pcis;

	pdev = from;
	class >>= 8;
	if (class == PCI_CLASS_BRIDGE_ISA) {
		pcis = PCIS_BRIDGE_ISA;
		pcic = PCIC_BRIDGE;
	} else if (class == PCI_CLASS_DISPLAY_VGA) {
		pcis = PCIS_DISPLAY_VGA;
		pcic = PCIC_DISPLAY;
	} else if (class == PCI_CLASS_DISPLAY_OTHER) {
		pcis = PCIS_DISPLAY_OTHER;
		pcic = PCIC_DISPLAY;
	} else {
		log(LOG_WARNING, "unrecognized class %x in %s\n", class, __FUNCTION__);
		BACKTRACE();
		return (NULL);
	}

	if (pdev != NULL) {
		dev = pdev->dev.bsddev;
	} else
		dev = NULL;

	dev = pci_find_class(pcic, pcis, dev);
	if (dev == NULL)
		return (NULL);

	if (pdev == NULL)
		pdev = malloc(sizeof(*pdev), M_DEVBUF, M_WAITOK|M_ZERO);

	/* XXX do we need to initialize pdev more here ? */
	pdev->devfn = PCI_DEVFN(pci_get_slot(dev), pci_get_function(dev));
	pdev->vendor = pci_get_vendor(dev);
	pdev->device = pci_get_device(dev);
	pdev->dev.bsddev = dev;
	if (from == NULL) {
		pbus = malloc(sizeof(*pbus), M_DEVBUF, M_WAITOK|M_ZERO);
		pbus->self = pdev;
		pdev->bus = pbus;
	}
	pdev->bus->number = pci_get_bus(dev);
	return (pdev);
}

static int
is_vga(device_t dev)
{
	device_t parent;
	devclass_t dc;

	parent = device_get_parent(dev);
	dc = device_get_devclass(parent);

	return (strcmp(devclass_get_name(dc), "vgapci") == 0);
}

void *
pci_map_rom(struct pci_dev *pdev, size_t *size)
{
	int rid;
	struct resource *res;
	device_t dev;

	dev = pdev->dev.bsddev;
#if defined(__amd64__) || defined(__i386__)
	if (vga_pci_is_boot_display(dev) || is_vga(dev)) {
		/*
		 * On x86, the System BIOS copy the default display
		 * device's Video BIOS at a fixed location in system
		 * memory (0xC0000, 128 kBytes long) at boot time.
		 *
		 * We use this copy for the default boot device, because
		 * the original ROM may not be valid after boot.
		 */

		*size = VGA_PCI_BIOS_SHADOW_SIZE;
		return (pmap_mapbios(VGA_PCI_BIOS_SHADOW_ADDR, *size));
	}
#endif

	rid = PCIR_BIOS;
	if ((res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, 0, ~0, 1, RF_ACTIVE)) == NULL)
		return (NULL);

	pdev->pcir.r[LINUXKPI_BIOS] = res;
	pdev->pcir.rid[LINUXKPI_BIOS] = rid;
	pdev->pcir.type[LINUXKPI_BIOS] = SYS_RES_MEMORY;
	pdev->pcir.map[LINUXKPI_BIOS] = rman_get_virtual(res);
	device_printf(dev, "bios size %lx bios addr %p\n", rman_get_size(res), rman_get_virtual(res));
	*size = rman_get_size(res);
	return (rman_get_virtual(res));
}

void
pci_unmap_rom(struct pci_dev *pdev, u8 *bios)
{
	device_t dev;
	struct resource *res;

	if (bios == NULL)
		return;
	dev = pdev->dev.bsddev;

#if defined(__amd64__) || defined(__i386__)
	if (vga_pci_is_boot_display(dev) || is_vga(dev)) {
		/* We mapped the BIOS shadow copy located at 0xC0000. */
		pmap_unmapdev((vm_offset_t)bios, VGA_PCI_BIOS_SHADOW_SIZE);

		return;
	}
#endif
	res = pdev->pcir.r[LINUXKPI_BIOS];
	pdev->pcir.r[LINUXKPI_BIOS] = NULL;
	pdev->pcir.rid[LINUXKPI_BIOS] = -1;
	pdev->pcir.type[LINUXKPI_BIOS] = -1;
	pdev->pcir.map[LINUXKPI_BIOS] = NULL;
	MPASS(res != NULL);
	bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BIOS, res);
}

void *
pci_platform_rom(struct pci_dev *pdev, size_t *size)
{
	return (NULL);
}

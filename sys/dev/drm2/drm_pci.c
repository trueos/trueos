/* drm_pci.h -- PCI DMA memory management wrappers for DRM -*- linux-c -*- */
/**
 * \file drm_pci.c
 * \brief Functions and ioctls to manage PCI memory
 *
 * \warning These interfaces aren't stable yet.
 *
 * \todo Implement the remaining ioctl's for the PCI pools.
 * \todo The wrappers here are so thin that they would be better off inlined..
 *
 * \author José Fonseca <jrfonseca@tungstengraphics.com>
 * \author Leif Delgass <ldelgass@retinalburn.net>
 */

/*
 * Copyright 2003 José Fonseca.
 * Copyright 2003 Leif Delgass.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/drm2/drmP.h>

static int drm_msi = 1;	/* Enable by default. */
SYSCTL_NODE(_hw, OID_AUTO, drm, CTLFLAG_RW, NULL, "DRM device");
SYSCTL_INT(_hw_drm, OID_AUTO, msi, CTLFLAG_RDTUN, &drm_msi, 1,
    "Enable MSI interrupts for drm devices");

/**********************************************************************/
/** \name PCI memory */
/*@{*/

static void
drm_pci_busdma_callback(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	drm_dma_handle_t *dmah = arg;

	if (error != 0)
		return;

	KASSERT(nsegs == 1, ("drm_pci_busdma_callback: bad dma segment count"));
	dmah->busaddr = segs[0].ds_addr;
}

/**
 * \brief Allocate a PCI consistent memory block, for DMA.
 */
drm_dma_handle_t *drm_pci_alloc(struct drm_device * dev, size_t size,
    size_t align)
{
	drm_dma_handle_t *dmah;
	int ret;

	/* Need power-of-two alignment, so fail the allocation if it isn't. */
	if ((align & (align - 1)) != 0) {
		DRM_ERROR("drm_pci_alloc with non-power-of-two alignment %d\n",
		    (int)align);
		return NULL;
	}

	dmah = malloc(sizeof(drm_dma_handle_t), DRM_MEM_DMA, M_ZERO | M_NOWAIT);
	if (dmah == NULL)
		return NULL;

	/* Make sure we aren't holding mutexes here */
	mtx_assert(&dev->dma_lock, MA_NOTOWNED);
	if (mtx_owned(&dev->dma_lock))
	    DRM_ERROR("called while holding dma_lock\n");

	ret = bus_dma_tag_create(
	    bus_get_dma_tag(dev->dev->bsddev), /* parent */
	    align, 0, /* align, boundary */
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, /* lowaddr, highaddr */
	    NULL, NULL, /* filtfunc, filtfuncargs */
	    size, 1, size, /* maxsize, nsegs, maxsegsize */
	    0, NULL, NULL, /* flags, lockfunc, lockfuncargs */
	    &dmah->tag);
	if (ret != 0) {
		free(dmah, DRM_MEM_DMA);
		return NULL;
	}

	ret = bus_dmamem_alloc(dmah->tag, (void **)&dmah->vaddr,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO | BUS_DMA_NOCACHE, &dmah->map);
	if (ret != 0) {
		bus_dma_tag_destroy(dmah->tag);
		free(dmah, DRM_MEM_DMA);
		return NULL;
	}

	ret = bus_dmamap_load(dmah->tag, dmah->map, dmah->vaddr, size,
	    drm_pci_busdma_callback, dmah, BUS_DMA_NOWAIT);
	if (ret != 0) {
		bus_dmamem_free(dmah->tag, dmah->vaddr, dmah->map);
		bus_dma_tag_destroy(dmah->tag);
		free(dmah, DRM_MEM_DMA);
		return NULL;
	}

	return dmah;
}

EXPORT_SYMBOL(drm_pci_alloc);

/**
 * \brief Free a PCI consistent memory block without freeing its descriptor.
 *
 * This function is for internal use in the Linux-specific DRM core code.
 */
void __drm_pci_free(struct drm_device * dev, drm_dma_handle_t * dmah)
{
	if (dmah == NULL)
		return;

	bus_dmamap_unload(dmah->tag, dmah->map);
	bus_dmamem_free(dmah->tag, dmah->vaddr, dmah->map);
	bus_dma_tag_destroy(dmah->tag);
}

/**
 * \brief Free a PCI consistent memory block
 */
void drm_pci_free(struct drm_device * dev, drm_dma_handle_t * dmah)
{
	__drm_pci_free(dev, dmah);
	kfree(dmah);
}

EXPORT_SYMBOL(drm_pci_free);

static int drm_get_pci_domain(struct drm_device *dev)
{
	return dev->pci_domain;
}

static int drm_pci_get_irq(struct drm_device *dev)
{

	return (dev->pdev->irq);
}

static const char *drm_pci_get_name(struct drm_device *dev)
{
	return dev->driver->name;
}

int drm_pci_set_busid(struct drm_device *dev, struct drm_master *master)
{
	int len, ret;
	master->unique_len = 40;
	master->unique_size = master->unique_len;
	master->unique = kmalloc(master->unique_size, GFP_KERNEL);
	if (master->unique == NULL)
		return -ENOMEM;


	len = snprintf(master->unique, master->unique_len,
		       "pci:%04x:%02x:%02x.%d",
		       dev->pci_domain,
		       dev->pci_bus,
		       dev->pci_slot,
		       dev->pci_func);

	if (len >= master->unique_len) {
		DRM_ERROR("buffer overflow");
		ret = -EINVAL;
		goto err;
	} else
		master->unique_len = len;

	return 0;
err:
	return ret;
}

int drm_pci_set_unique(struct drm_device *dev,
		       struct drm_master *master,
		       struct drm_unique *u)
{
	int domain, bus, slot, func, ret;

	master->unique_len = u->unique_len;
	master->unique_size = u->unique_len + 1;
	master->unique = kmalloc(master->unique_size, GFP_KERNEL);
	if (!master->unique) {
		ret = -ENOMEM;
		goto err;
	}

	if (copy_from_user(master->unique, u->unique, master->unique_len)) {
		ret = -EFAULT;
		goto err;
	}

	master->unique[master->unique_len] = '\0';

	/* Return error if the busid submitted doesn't match the device's actual
	 * busid.
	 */
	ret = sscanf(master->unique, "PCI:%d:%d:%d", &bus, &slot, &func);
	if (ret != 3) {
		ret = -EINVAL;
		goto err;
	}

	domain = bus >> 8;
	bus &= 0xff;

	if ((domain != dev->pci_domain) ||
	    (bus != dev->pci_bus) ||
	    (slot != dev->pci_slot) ||
	    (func != dev->pci_func)) {
		ret = -EINVAL;
		goto err;
	}
	return 0;
err:
	return ret;
}


static int drm_pci_irq_by_busid(struct drm_device *dev, struct drm_irq_busid *p)
{
	if ((p->busnum >> 8) != drm_get_pci_domain(dev) ||
	    (p->busnum & 0xff) != dev->pci_bus ||
	    p->devnum != dev->pci_slot || p->funcnum != dev->pci_func)
		return -EINVAL;

	p->irq = dev->irq;

	DRM_DEBUG("%d:%d:%d => IRQ %d\n", p->busnum, p->devnum, p->funcnum,
		  p->irq);
	return 0;
}

int drm_pci_agp_init(struct drm_device *dev)
{
	if (drm_core_has_AGP(dev)) {
		if (drm_pci_device_is_agp(dev))
			dev->agp = drm_agp_init(dev);
		if (drm_core_check_feature(dev, DRIVER_REQUIRE_AGP)
		    && (dev->agp == NULL)) {
			DRM_ERROR("Cannot initialize the agpgart module.\n");
			return -EINVAL;
		}
		if (drm_core_has_MTRR(dev)) {
			if (dev->agp && dev->agp->agp_info.ai_aperture_base != 0) {
				if (mtrr_add(dev->agp->agp_info.ai_aperture_base,
					     dev->agp->agp_info.ai_aperture_size, MTRR_TYPE_WRCOMB, 1) == 0)
					dev->agp->agp_mtrr = 1;
				else
					dev->agp->agp_mtrr = -1;
			}
		}
	}
	return 0;
}

static struct drm_bus drm_pci_bus = {
	.bus_type = DRIVER_BUS_PCI,
	.get_irq = drm_pci_get_irq,
	.get_name = drm_pci_get_name,
	.set_busid = drm_pci_set_busid,
	.set_unique = drm_pci_set_unique,
	.irq_by_busid = drm_pci_irq_by_busid,
	.agp_init = drm_pci_agp_init,
};

/**
 * Register.
 *
 * \param pdev - PCI device structure
 * \param ent entry from the PCI ID table with device type flags
 * \return zero on success or a negative number on failure.
 *
 * Attempt to gets inter module "drm" information. If we are first
 * then register the character device and inter module information.
 * Try and register, if we fail to register, backout previous work.
 */
int drm_get_pci_dev(struct pci_dev *pdev, const struct pci_device_id *ent,
		    struct drm_driver *driver)
{
	struct drm_device *dev;
	int ret;

	DRM_DEBUG("\n");

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	ret = pci_enable_device(pdev);
	if (ret)
		goto err_g1;

	dev->pdev = pdev;
	dev->dev = &pdev->dev;

	dev->pci_device = pdev->device;
	dev->pci_vendor = pdev->vendor;

#ifdef __alpha__
	dev->hose = pdev->sysdata;
#endif

	mutex_lock(&drm_global_mutex);

	if ((ret = drm_fill_in_dev(dev, ent, driver))) {
		printk(KERN_ERR "DRM: Fill_in_dev failed.\n");
		goto err_g2;
	}

	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		pci_set_drvdata(pdev, dev);
		ret = drm_get_minor(dev, &dev->control, DRM_MINOR_CONTROL);
		if (ret)
			goto err_g2;
	}

	if ((ret = drm_get_minor(dev, &dev->primary, DRM_MINOR_LEGACY)))
		goto err_g3;

	if (dev->driver->load) {
		ret = dev->driver->load(dev, ent->driver_data);
		if (ret)
			goto err_g4;
	}

	/* setup the grouping for the legacy output */
	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		ret = drm_mode_group_init_legacy_group(dev,
						&dev->primary->mode_group);
		if (ret)
			goto err_g4;
	}

	list_add_tail(&dev->driver_item, &driver->device_list);

	DRM_INFO("Initialized %s %d.%d.%d %s for %s on minor %d\n",
		 driver->name, driver->major, driver->minor, driver->patchlevel,
		 driver->date, pci_name(pdev), dev->primary->index);

	mutex_unlock(&drm_global_mutex);
	return 0;

err_g4:
	drm_put_minor(&dev->primary);
err_g3:
	if (drm_core_check_feature(dev, DRIVER_MODESET))
		drm_put_minor(&dev->control);
err_g2:
	pci_disable_device(pdev);
err_g1:
	kfree(dev);
	mutex_unlock(&drm_global_mutex);
	return ret;
}
EXPORT_SYMBOL(drm_get_pci_dev);

int
drm_pci_enable_msi(struct drm_device *dev)
{
	int msicount, ret;

	if (!drm_msi)
		return (-ENOENT);

	msicount = pci_msi_count(dev->dev->bsddev);
	DRM_DEBUG("MSI count = %d\n", msicount);
	if (msicount > 1)
		msicount = 1;

	ret = pci_alloc_msi(dev->dev->bsddev, &msicount);
	if (ret == 0) {
		DRM_INFO("MSI enabled %d message(s)\n", msicount);
		dev->msi_enabled = 1;
		dev->irqrid = 1;
	}

	return (-ret);
}

extern devclass_t drm_devclass;
/**
 * PCI device initialization. Called direct from modules at load time.
 *
 * \return zero on success or a negative number on failure.
 *
 * Initializes a drm_device structures,registering the
 * stubs and initializing the AGP device.
 *
 * Expands the \c DRIVER_PREINIT and \c DRIVER_POST_INIT macros before and
 * after the initialization for driver customization.
 */
int drm_pci_init(struct drm_driver *driver, struct pci_driver *pdriver)
{
	struct pci_dev *pdev = NULL;
	const struct pci_device_id *pid;
	int i;

	DRM_DEBUG("\n");

	INIT_LIST_HEAD(&driver->device_list);
	driver->kdriver.pci = pdriver;
	driver->bus = &drm_pci_bus;
	pdriver->busname = "vgapci";
	pdriver->bsdclass = &drm_devclass;
	pdriver->name = "drmn";
	
	if (driver->driver_features & DRIVER_MODESET)
		return pci_register_driver(pdriver);

	printf("not driver modeset!!!");
	return (-ENOTSUP);
#ifdef __linux__	
	/* If not using KMS, fall back to stealth mode manual scanning. */
	for (i = 0; pdriver->id_table[i].vendor != 0; i++) {
		pid = &pdriver->id_table[i];

		/* Loop around setting up a DRM device for each PCI device
		 * matching our ID and device class.  If we had the internal
		 * function that pci_get_subsys and pci_get_class used, we'd
		 * be able to just pass pid in instead of doing a two-stage
		 * thing.
		 */
		pdev = NULL;
		while ((pdev =
			pci_get_subsys(pid->vendor, pid->device, pid->subvendor,
				       pid->subdevice, pdev)) != NULL) {
			if ((pdev->class & pid->class_mask) != pid->class)
				continue;

			/* stealth mode requires a manual probe */
			pci_dev_get(pdev);
			drm_get_pci_dev(pdev, pid, driver);
		}
	}
#endif	
	return 0;
}
void
drm_pci_disable_msi(struct drm_device *dev)
{

	if (!dev->msi_enabled)
		return;

	pci_release_msi(dev->dev->bsddev);
	dev->msi_enabled = 0;
	dev->irqrid = 0;
}

int drm_pcie_get_speed_cap_mask(struct drm_device *dev, u32 *mask)
{
	device_t root;
	int pos;
	u32 lnkcap = 0, lnkcap2 = 0;

	*mask = 0;
	if (!drm_pci_device_is_pcie(dev))
		return -EINVAL;

	root =
	    device_get_parent( /* pcib             */
	    device_get_parent( /* `-- pci          */
	    device_get_parent( /*     `-- vgapci   */
	    dev->dev->bsddev)));       /*         `-- drmn */

	pos = 0;
	pci_find_cap(root, PCIY_EXPRESS, &pos);
	if (!pos)
		return -EINVAL;

	/* we've been informed via and serverworks don't make the cut */
	if (pci_get_vendor(root) == PCI_VENDOR_ID_VIA ||
	    pci_get_vendor(root) == PCI_VENDOR_ID_SERVERWORKS)
		return -EINVAL;

	lnkcap = pci_read_config(root, pos + PCIER_LINK_CAP, 4);
	lnkcap2 = pci_read_config(root, pos + PCIER_LINK_CAP2, 4);

	lnkcap &= PCIEM_LINK_CAP_MAX_SPEED;
	lnkcap2 &= 0xfe;

#define	PCI_EXP_LNKCAP2_SLS_2_5GB 0x02	/* Supported Link Speed 2.5GT/s */
#define	PCI_EXP_LNKCAP2_SLS_5_0GB 0x04	/* Supported Link Speed 5.0GT/s */
#define	PCI_EXP_LNKCAP2_SLS_8_0GB 0x08	/* Supported Link Speed 8.0GT/s */

	if (lnkcap2) { /* PCIE GEN 3.0 */
		if (lnkcap2 & PCI_EXP_LNKCAP2_SLS_2_5GB)
			*mask |= DRM_PCIE_SPEED_25;
		if (lnkcap2 & PCI_EXP_LNKCAP2_SLS_5_0GB)
			*mask |= DRM_PCIE_SPEED_50;
		if (lnkcap2 & PCI_EXP_LNKCAP2_SLS_8_0GB)
			*mask |= DRM_PCIE_SPEED_80;
	} else {
		if (lnkcap & 1)
			*mask |= DRM_PCIE_SPEED_25;
		if (lnkcap & 2)
			*mask |= DRM_PCIE_SPEED_50;
	}

	DRM_INFO("probing gen 2 caps for device %x:%x = %x/%x\n", pci_get_vendor(root), pci_get_device(root), lnkcap, lnkcap2);
	return 0;
}
EXPORT_SYMBOL(drm_pcie_get_speed_cap_mask);


/*@}*/
void drm_pci_exit(struct drm_driver *driver, struct pci_driver *pdriver)
{
	struct drm_device *dev, *tmp;
	DRM_DEBUG("\n");

	if (driver->driver_features & DRIVER_MODESET) {
		pci_unregister_driver(pdriver);
	} else {
		list_for_each_entry_safe(dev, tmp, &driver->device_list, driver_item)
			drm_put_dev(dev);
	}
	DRM_INFO("Module unloaded\n");
}
EXPORT_SYMBOL(drm_pci_exit);


#if 0
static int drm_pci_get_irq(struct drm_device *dev)
{
	if (dev->irqr)
		return (dev->irq);

	dev->irqr = bus_alloc_resource_any(dev->dev->bsddev, SYS_RES_IRQ,
	    &dev->irqrid, RF_SHAREABLE);
	if (!dev->irqr) {
		dev_err(dev->dev->bsddev, "Failed to allocate IRQ\n");
		return (0);
	}

	dev->irq = (int) rman_get_start(dev->irqr);

	return (dev->irq);
}

static void drm_pci_free_irq(struct drm_device *dev)
{
	if (dev->irqr == NULL)
		return;

	bus_release_resource(dev->dev->bsddev, SYS_RES_IRQ,
	    dev->irqrid, dev->irqr);

	dev->irqr = NULL;
	dev->irq = 0;
}
#endif

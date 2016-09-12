
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <drm/drmP.h>

#include <dev/agp/agpreg.h>
#include <dev/pci/pcireg.h>

devclass_t drm_devclass;

MALLOC_DEFINE(DRM_MEM_DMA, "drm_dma", "DRM DMA Data Structures");
MALLOC_DEFINE(DRM_MEM_DRIVER, "drm_driver", "DRM DRIVER Data Structures");
MALLOC_DEFINE(DRM_MEM_KMS, "drm_kms", "DRM KMS Data Structures");

const char *fb_mode_option = NULL;

static const drm_pci_id_list_t *
drm_find_description(int vendor, int device, const drm_pci_id_list_t *idlist)
{
	int i = 0;

	for (i = 0; idlist[i].vendor != 0; i++) {
		if ((idlist[i].vendor == vendor) &&
		    ((idlist[i].device == device) ||
		    (idlist[i].device == 0))) {
			return (&idlist[i]);
		}
	}
	return (NULL);
}


/*
 * drm_probe_helper: called by a driver at the end of its probe
 * method.
 */
int
drm_probe_helper(device_t kdev, const drm_pci_id_list_t *idlist)
{
	const drm_pci_id_list_t *id_entry;
	int vendor, device;

	vendor = pci_get_vendor(kdev);
	device = pci_get_device(kdev);

	if (pci_get_class(kdev) != PCIC_DISPLAY ||
	    (pci_get_subclass(kdev) != PCIS_DISPLAY_VGA &&
	     pci_get_subclass(kdev) != PCIS_DISPLAY_OTHER))
		return (-ENXIO);

	id_entry = drm_find_description(vendor, device, idlist);
	if (id_entry != NULL) {
		if (device_get_desc(kdev) == NULL) {
			DRM_DEBUG("%s desc: %s\n",
			    device_get_nameunit(kdev), id_entry->name);
			device_set_desc(kdev, id_entry->name);
		}
		return (0);
	}

	return (-ENXIO);
}

int
drm_generic_suspend(device_t kdev)
{
	struct drm_device *dev;
	int error;

	DRM_DEBUG_KMS("Starting suspend\n");

	dev = device_get_softc(kdev);
	if (dev->driver->suspend) {
		pm_message_t state;

		state.event = PM_EVENT_SUSPEND;
		error = -dev->driver->suspend(dev, state);
		if (error)
			goto out;
	}

	error = bus_generic_suspend(kdev);

out:
	DRM_DEBUG_KMS("Finished suspend: %d\n", error);

	return error;
}

int
drm_generic_resume(device_t kdev)
{
	struct drm_device *dev;
	int error;

	DRM_DEBUG_KMS("Starting resume\n");

	dev = device_get_softc(kdev);
	if (dev->driver->resume) {
		error = -dev->driver->resume(dev);
		if (error)
			goto out;
	}

	error = bus_generic_resume(kdev);

out:
	DRM_DEBUG_KMS("Finished resume: %d\n", error);

	return error;
}

int
drm_generic_detach(device_t kdev)
{
	struct drm_device *dev;
	int i;

	dev = device_get_softc(kdev);

	drm_put_dev(dev);
#if 0
	/* Clean up PCI resources allocated by drm_bufs.c.  We're not really
	 * worried about resource consumption while the DRM is inactive (between
	 * lastclose and firstopen or unload) because these aren't actually
	 * taking up KVA, just keeping the PCI resource allocated.
	 */
	for (i = 0; i < DRM_MAX_PCI_RESOURCE; i++) {
		if (dev->pcir[i] == NULL)
			continue;
		bus_release_resource(dev->dev->bsddev, SYS_RES_MEMORY,
		    dev->pcirid[i], dev->pcir[i]);
		dev->pcir[i] = NULL;
	}

#endif	
	if (pci_disable_busmaster(dev->dev->bsddev))
		DRM_ERROR("Request to disable bus-master failed.\n");

	return (0);
}

static int
drm_device_find_capability(struct drm_device *dev, int cap)
{

	return (pci_find_cap(dev->dev->bsddev, cap, NULL) == 0);
}

#if 0
int
drm_pci_device_is_agp(struct drm_device *dev)
{
	if (dev->driver->device_is_agp != NULL) {
		int ret;

		/* device_is_agp returns a tristate, 0 = not AGP, 1 = definitely
		 * AGP, 2 = fall back to PCI capability
		 */
		ret = (*dev->driver->device_is_agp)(dev);
		if (ret != DRM_MIGHT_BE_AGP)
			return ret;
	}

	return (drm_device_find_capability(dev, PCIY_AGP));
}
#endif

int
drm_pci_device_is_pcie(struct drm_device *dev)
{

	return (drm_device_find_capability(dev, PCIY_EXPRESS));
}

#if 0
void
drm_clflush_pages(vm_page_t *pages, unsigned long num_pages)
{

#if defined(__i386__) || defined(__amd64__)
	pmap_invalidate_cache_pages(pages, num_pages);
#else
	DRM_ERROR("drm_clflush_pages not implemented on this architecture");
#endif
}

void
drm_clflush_sg(struct sg_table *st)
{
#if defined(__i386__) || defined(__amd64__)
		struct scatterlist *sg;
		int i;

		mb();
		for_each_sg(st->sgl, sg, st->nents, i)
			drm_clflush_pages(&sg_page(sg), 1);
		mb();

		return;
#else
	printk(KERN_ERR "Architecture has no drm_cache.c support\n");
	WARN_ON_ONCE(1);
#endif
}

void
drm_clflush_virt_range(void *addr, unsigned long length)
{

#if defined(__i386__) || defined(__amd64__)
	pmap_invalidate_cache_range((vm_offset_t)addr,
	    (vm_offset_t)addr + length, TRUE);
#else
	DRM_ERROR("drm_clflush_virt_range not implemented on this architecture");
#endif
}
#endif

void
hex_dump_to_buffer(const void *buf, size_t len, int rowsize, int groupsize,
    char *linebuf, size_t linebuflen, bool ascii __unused)
{
	int i, j, c;

	i = j = 0;

	while (i < len && j <= linebuflen) {
		c = ((const char *)buf)[i];

		if (i != 0) {
			if (i % rowsize == 0) {
				/* Newline required. */
				sprintf(linebuf + j, "\n");
				++j;
			} else if (i % groupsize == 0) {
				/* Space required. */
				sprintf(linebuf + j, " ");
				++j;
			}
		}

		if (j > linebuflen - 4)
			break;

		sprintf(linebuf + j, "%02X", c);
		j += 2;

		++i;
	}

	if (j <= linebuflen)
		sprintf(linebuf + j, "\n");
}
static void
drm_kqfilter_detach(struct knote *kn)
{
	struct linux_file *filp = kn->kn_hook;

	spin_lock(&filp->f_lock);
	knlist_remove(&filp->f_selinfo.si_note, kn, 1);
	spin_unlock(&filp->f_lock);
}

#ifdef DRM_KQ_DEBUG
#define PRINTF printf
#else
#define PRINTF(...)
#endif

static int
drm_kqfilter_read(struct knote *kn, long hint)
{

	struct file *filp = kn->kn_hook;
	struct drm_file *file_priv = filp->private_data;
	struct drm_device *dev = file_priv->minor->dev;
	struct drm_pending_event *e = NULL;
	struct drm_pending_event *et = NULL;
	ssize_t ret = 0;

	PRINTF("%s: pending(hint) = %ld\n",__func__,hint);
	spin_lock_irq(&dev->event_lock);
	list_for_each_entry_safe(e, et, &file_priv->event_list, link) {
		ret += e->event->length;
	}
	spin_unlock_irq(&dev->event_lock);
	kn->kn_data = ret;
	PRINTF("%s: current=%p. event list size: %d bytes. empty=%d\n",
		   __func__,current,ret,list_empty(&file_priv->event_list));
	return (ret > 0);

}

static struct filterops drm_kqfiltops_read = {
	.f_isfd = 1,
	.f_detach = drm_kqfilter_detach,
	.f_event = drm_kqfilter_read,
};

void
drm_kqregister(struct linux_file *filp)
{
	filp->f_kqfiltops = &drm_kqfiltops_read;
}

#define to_drm_minor(d) container_of(d, struct drm_minor, kdev)
#define to_drm_connector(d) container_of(d, struct drm_connector, kdev)

static struct device_type drm_sysfs_device_minor = {
	.name = "drm_minor"
};


static char *drm_devnode(struct device *dev, umode_t *mode)
{
	return kasprintf(GFP_KERNEL, "dri/%s", dev_name(dev));
}

static int
drm_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		TUNABLE_INT_FETCH("drm.debug", &drm_debug);
		break;
	}
	return (0);
}

static moduledata_t drm_mod = {
	"drmn",
	drm_modevent,
	0
};

DECLARE_MODULE(drmn, drm_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(drmn, 1);
MODULE_DEPEND(drmn, agp, 1, 1, 1);
MODULE_DEPEND(drmn, pci, 1, 1, 1);
MODULE_DEPEND(drmn, mem, 1, 1, 1);
MODULE_DEPEND(drmn, linuxkpi, 1, 1, 1);
MODULE_DEPEND(drmn, debugfs, 1, 1, 1);

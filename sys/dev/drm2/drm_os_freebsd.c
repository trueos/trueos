
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <drm/drmP.h>

#include <dev/agp/agpreg.h>
#include <dev/pci/pcireg.h>

devclass_t drm_devclass;

MALLOC_DEFINE(DRM_MEM_DMA, "drm_dma", "DRM DMA Data Structures");
MALLOC_DEFINE(DRM_MEM_DRIVER, "drm_driver", "DRM DRIVER Data Structures");
MALLOC_DEFINE(DRM_MEM_FILES, "drm_files", "DRM FILE Data Structures");
MALLOC_DEFINE(DRM_MEM_BUFLISTS, "drm_buflists", "DRM BUFLISTS Data Structures");
MALLOC_DEFINE(DRM_MEM_AGPLISTS, "drm_agplists", "DRM AGPLISTS Data Structures");
MALLOC_DEFINE(DRM_MEM_CTXBITMAP, "drm_ctxbitmap",
    "DRM CTXBITMAP Data Structures");
MALLOC_DEFINE(DRM_MEM_HASHTAB, "drm_hashtab", "DRM HASHTABLE Data Structures");
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

#if 0
/*
 * drm_attach_helper: called by a driver at the end of its attach
 * method.
 */
int
drm_attach_helper(device_t kdev, const drm_pci_id_list_t *idlist,
    struct drm_driver *driver)
{
	struct drm_device *dev;
	int vendor, device;
	int ret;

	dev = device_get_softc(kdev);

	vendor = pci_get_vendor(kdev);
	device = pci_get_device(kdev);
	dev->id_entry = drm_find_description(vendor, device, idlist);

	ret = drm_get_pci_dev(kdev, dev, driver);

	return (ret);
}

#endif

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

	if (pci_disable_busmaster(dev->dev->bsddev))
		DRM_ERROR("Request to disable bus-master failed.\n");

	return (0);
}

int
drm_add_busid_modesetting(struct drm_device *dev, struct sysctl_ctx_list *ctx,
    struct sysctl_oid *top)
{
	struct sysctl_oid *oid;

	snprintf(dev->busid_str, sizeof(dev->busid_str),
	     "pci:%04x:%02x:%02x.%d", dev->pci_domain, dev->pci_bus,
	     dev->pci_slot, dev->pci_func);
	oid = SYSCTL_ADD_STRING(ctx, SYSCTL_CHILDREN(top), OID_AUTO, "busid",
	    CTLFLAG_RD, dev->busid_str, 0, NULL);
	if (oid == NULL)
		return (-ENOMEM);
	dev->modesetting = (dev->driver->driver_features & DRIVER_MODESET) != 0;
	oid = SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(top), OID_AUTO,
	    "modesetting", CTLFLAG_RD, &dev->modesetting, 0, NULL);
	if (oid == NULL)
		return (-ENOMEM);

	return (0);
}

static int
drm_device_find_capability(struct drm_device *dev, int cap)
{

	return (pci_find_cap(dev->dev->bsddev, cap, NULL) == 0);
}

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

int
drm_pci_device_is_pcie(struct drm_device *dev)
{

	return (drm_device_find_capability(dev, PCIY_EXPRESS));
}

static bool
dmi_found(const struct dmi_system_id *dsi)
{
	char *hw_vendor, *hw_prod;
	int i, slot;
	bool res;

	hw_vendor = kern_getenv("smbios.planar.maker");
	hw_prod = kern_getenv("smbios.planar.product");
	res = true;
	for (i = 0; i < nitems(dsi->matches); i++) {
		slot = dsi->matches[i].slot;
		switch (slot) {
		case DMI_NONE:
			break;
		case DMI_SYS_VENDOR:
		case DMI_BOARD_VENDOR:
			if (hw_vendor != NULL &&
			    !strcmp(hw_vendor, dsi->matches[i].substr)) {
				break;
			} else {
				res = false;
				goto out;
			}
		case DMI_PRODUCT_NAME:
		case DMI_BOARD_NAME:
			if (hw_prod != NULL &&
			    !strcmp(hw_prod, dsi->matches[i].substr)) {
				break;
			} else {
				res = false;
				goto out;
			}
		default:
			res = false;
			goto out;
		}
	}
out:
	freeenv(hw_vendor);
	freeenv(hw_prod);

	return (res);
}

bool
dmi_check_system(const struct dmi_system_id *sysid)
{
	const struct dmi_system_id *dsi;
	bool res;

	for (res = false, dsi = sysid; dsi->matches[0].slot != 0 ; dsi++) {
		if (dmi_found(dsi)) {
			res = true;
			if (dsi->callback != NULL && dsi->callback(dsi))
				break;
		}
	}
	return (res);
}

int
drm_mtrr_add(unsigned long offset, unsigned long size, unsigned int flags)
{
	int act;
	struct mem_range_desc mrdesc;

	mrdesc.mr_base = offset;
	mrdesc.mr_len = size;
	mrdesc.mr_flags = flags;
	act = MEMRANGE_SET_UPDATE;
	strlcpy(mrdesc.mr_owner, "drm", sizeof(mrdesc.mr_owner));
	return (-mem_range_attr_set(&mrdesc, &act));
}

int
drm_mtrr_del(int handle __unused, unsigned long offset, unsigned long size,
    unsigned int flags)
{
	int act;
	struct mem_range_desc mrdesc;

	mrdesc.mr_base = offset;
	mrdesc.mr_len = size;
	mrdesc.mr_flags = flags;
	act = MEMRANGE_SET_REMOVE;
	strlcpy(mrdesc.mr_owner, "drm", sizeof(mrdesc.mr_owner));
	return (-mem_range_attr_set(&mrdesc, &act));
}

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
drm_clflush_virt_range(char *addr, unsigned long length)
{

#if defined(__i386__) || defined(__amd64__)
	pmap_invalidate_cache_range((vm_offset_t)addr,
	    (vm_offset_t)addr + length, TRUE);
#else
	DRM_ERROR("drm_clflush_virt_range not implemented on this architecture");
#endif
}

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

		if (j > linebuflen - 1)
			break;

		sprintf(linebuf + j, "%02X", c);
		j += 2;

		++i;
	}

	if (j <= linebuflen)
		sprintf(linebuf + j, "\n");
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


/**
 * drm_sysfs_create - create a struct drm_sysfs_class structure
 * @owner: pointer to the module that is to "own" this struct drm_sysfs_class
 * @name: pointer to a string for the name of this class.
 *
 * This is used to create DRM class pointer that can then be used
 * in calls to drm_sysfs_device_add().
 *
 * Note, the pointer created here is to be destroyed when finished by making a
 * call to drm_sysfs_destroy().
 */
struct class *drm_sysfs_create(struct module *owner, char *name)
{
	struct class *class;
	int err;

	class = class_create(owner, name);
	if (IS_ERR(class)) {
		err = PTR_ERR(class);
		goto err_out;
	}
#ifdef __linux__
	class->suspend = drm_class_suspend;
	class->resume = drm_class_resume;

	err = class_create_file(class, &class_attr_version.attr);
	if (err)
		goto err_out_class;
#endif
	class->devnode = drm_devnode;

	return class;

err_out_class:
	class_destroy(class);
err_out:
	return ERR_PTR(err);
}

/**
 * drm_sysfs_destroy - destroys DRM class
 *
 * Destroy the DRM device class.
 */
void drm_sysfs_destroy(void)
{
	if ((drm_class == NULL) || (IS_ERR(drm_class)))
		return;
#ifdef __linux__	
	class_remove_file(drm_class, &class_attr_version.attr);
#endif	
	class_destroy(drm_class);
	drm_class = NULL;
}

/**
 * drm_sysfs_device_release - do nothing
 * @dev: Linux device
 *
 * Normally, this would free the DRM device associated with @dev, along
 * with cleaning up any other stuff.  But we do that in the DRM core, so
 * this function can just return and hope that the core does its job.
 */
static void drm_sysfs_device_release(struct device *dev)
{
	memset(dev, 0, sizeof(struct device));
	return;
}


/**
 * drm_sysfs_connector_add - add a connector to sysfs
 * @connector: connector to add
 *
 * Create a connector device in sysfs, along with its associated connector
 * properties (so far, connection status, dpms, mode list & edid) and
 * generate a hotplug event so userspace knows there's a new connector
 * available.
 *
 * Note:
 * This routine should only be called *once* for each registered connector.
 * A second call for an already registered connector will trigger the BUG_ON
 * below.
 */
int drm_sysfs_connector_add(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	int attr_cnt = 0;
	int opt_cnt = 0;
	int i;
	int ret;

#ifdef notyet
	/* We shouldn't get called more than once for the same connector */
	BUG_ON(device_is_registered(&connector->kdev));
#endif	

	connector->kdev.parent = &dev->primary->kdev;
	connector->kdev.class = drm_class;
	connector->kdev.release = drm_sysfs_device_release;

	DRM_DEBUG("adding \"%s\" to sysfs\n",
		  drm_get_connector_name(connector));

	dev_set_name(&connector->kdev, "card%d-%s",
		     dev->primary->index, drm_get_connector_name(connector));
	ret = device_register(&connector->kdev);

	if (ret) {
		DRM_ERROR("failed to register connector device: %d\n", ret);
		goto out;
	}

	/* Standard attributes */
#ifdef __linux__
	for (attr_cnt = 0; attr_cnt < ARRAY_SIZE(connector_attrs); attr_cnt++) {
		ret = device_create_file(&connector->kdev, &connector_attrs[attr_cnt]);
		if (ret)
			goto err_out_files;
	}

	/* Optional attributes */
	/*
	 * In the long run it maybe a good idea to make one set of
	 * optionals per connector type.
	 */
	switch (connector->connector_type) {
		case DRM_MODE_CONNECTOR_DVII:
		case DRM_MODE_CONNECTOR_Composite:
		case DRM_MODE_CONNECTOR_SVIDEO:
		case DRM_MODE_CONNECTOR_Component:
		case DRM_MODE_CONNECTOR_TV:
			for (opt_cnt = 0; opt_cnt < ARRAY_SIZE(connector_attrs_opt1); opt_cnt++) {
				ret = device_create_file(&connector->kdev, &connector_attrs_opt1[opt_cnt]);
				if (ret)
					goto err_out_files;
			}
			break;
		default:
			break;
	}

	ret = sysfs_create_bin_file(&connector->kdev.kobj, &edid_attr);
	if (ret)
		goto err_out_files;

	/* Let userspace know we have a new connector */
	drm_sysfs_hotplug_event(dev);
#endif
	return 0;

err_out_files:
#ifdef __linux__	
	for (i = 0; i < opt_cnt; i++)
		device_remove_file(&connector->kdev, &connector_attrs_opt1[i]);
	for (i = 0; i < attr_cnt; i++)
		device_remove_file(&connector->kdev, &connector_attrs[i]);
#endif	
	device_unregister(&connector->kdev);

out:
	return ret;
}
EXPORT_SYMBOL(drm_sysfs_connector_add);

/**
 * drm_sysfs_connector_remove - remove an connector device from sysfs
 * @connector: connector to remove
 *
 * Remove @connector and its associated attributes from sysfs.  Note that
 * the device model core will take care of sending the "remove" uevent
 * at this time, so we don't need to do it.
 *
 * Note:
 * This routine should only be called if the connector was previously
 * successfully registered.  If @connector hasn't been registered yet,
 * you'll likely see a panic somewhere deep in sysfs code when called.
 */
void drm_sysfs_connector_remove(struct drm_connector *connector)
{
	int i;

	if (!connector->kdev.parent)
		return;
	DRM_DEBUG("removing \"%s\" from sysfs\n",
		  drm_get_connector_name(connector));
#ifdef __linux__	
	for (i = 0; i < ARRAY_SIZE(connector_attrs); i++)
		device_remove_file(&connector->kdev, &connector_attrs[i]);

	sysfs_remove_bin_file(&connector->kdev.kobj, &edid_attr);
#endif	
	device_unregister(&connector->kdev);
	connector->kdev.parent = NULL;
}
EXPORT_SYMBOL(drm_sysfs_connector_remove);

/**
 * drm_sysfs_device_add - adds a class device to sysfs for a character driver
 * @dev: DRM device to be added
 * @head: DRM head in question
 *
 * Add a DRM device to the DRM's device model class.  We use @dev's PCI device
 * as the parent for the Linux device, and make sure it has a file containing
 * the driver we're using (for userspace compatibility).
 */
int drm_sysfs_device_add(struct drm_minor *minor)
{
	int err;
	char *minor_str;

	minor->kdev.parent = minor->dev->dev;

	minor->kdev.class = drm_class;
	minor->kdev.release = drm_sysfs_device_release;
	minor->kdev.devt = minor->device;
	minor->kdev.type = &drm_sysfs_device_minor;
	if (minor->type == DRM_MINOR_CONTROL)
		minor_str = "controlD%d";
        else if (minor->type == DRM_MINOR_RENDER)
                minor_str = "renderD%d";
        else
                minor_str = "card%d";

	dev_set_name(&minor->kdev, minor_str, minor->index);

	err = device_register(&minor->kdev);
	if (err) {
		DRM_ERROR("device add failed: %d\n", err);
		goto err_out;
	}

	return 0;

err_out:
	return err;
}

/**
 * drm_sysfs_device_remove - remove DRM device
 * @dev: DRM device to remove
 *
 * This call unregisters and cleans up a class device that was created with a
 * call to drm_sysfs_device_add()
 */
void drm_sysfs_device_remove(struct drm_minor *minor)
{
	if (minor->kdev.parent)
		device_unregister(&minor->kdev);
	minor->kdev.parent = NULL;
}

#if DRM_LINUX

#include <sys/sysproto.h>

MODULE_DEPEND(DRIVER_NAME, linux, 1, 1, 1);

#define LINUX_IOCTL_DRM_MIN		0x6400
#define LINUX_IOCTL_DRM_MAX		0x64ff

static linux_ioctl_function_t drm_linux_ioctl;
static struct linux_ioctl_handler drm_handler = {drm_linux_ioctl,
    LINUX_IOCTL_DRM_MIN, LINUX_IOCTL_DRM_MAX};

/* The bits for in/out are switched on Linux */
#define LINUX_IOC_IN	IOC_OUT
#define LINUX_IOC_OUT	IOC_IN

static int
drm_linux_ioctl(DRM_STRUCTPROC *p, struct linux_ioctl_args* args)
{
	int error;
	int cmd = args->cmd;

	args->cmd &= ~(LINUX_IOC_IN | LINUX_IOC_OUT);
	if (cmd & LINUX_IOC_IN)
		args->cmd |= IOC_IN;
	if (cmd & LINUX_IOC_OUT)
		args->cmd |= IOC_OUT;

	error = ioctl(p, (struct ioctl_args *)args);

	return error;
}

/* Allocation of PCI memory resources (framebuffer, registers, etc.) for
 * drm_get_resource_*.  Note that they are not RF_ACTIVE, so there's no virtual
 * address for accessing them.  Cleaned up at unload.
 */
static int drm_alloc_resource(struct drm_device *dev, int resource)
{
	struct resource *res;
	int rid;

	if (resource >= DRM_MAX_PCI_RESOURCE) {
		DRM_ERROR("Resource %d too large\n", resource);
		return 1;
	}

	if (dev->pcir[resource] != NULL) {
		return 0;
	}

	rid = PCIR_BAR(resource);
	res = bus_alloc_resource_any(dev->dev->bsddev, SYS_RES_MEMORY, &rid,
	    RF_SHAREABLE);
	if (res == NULL) {
		DRM_ERROR("Couldn't find resource 0x%x\n", resource);
		return 1;
	}

	if (dev->pcir[resource] == NULL) {
		dev->pcirid[resource] = rid;
		dev->pcir[resource] = res;
	}

	return 0;
}

unsigned long drm_get_resource_start(struct drm_device *dev,
				     unsigned int resource)
{
	unsigned long start;

	mtx_lock(&dev->pcir_lock);

	if (drm_alloc_resource(dev, resource) != 0)
		return 0;

	start = rman_get_start(dev->pcir[resource]);

	mtx_unlock(&dev->pcir_lock);

	return (start);
}

unsigned long drm_get_resource_len(struct drm_device *dev,
				   unsigned int resource)
{
	unsigned long len;

	mtx_lock(&dev->pcir_lock);

	if (drm_alloc_resource(dev, resource) != 0)
		return 0;

	len = rman_get_size(dev->pcir[resource]);

	mtx_unlock(&dev->pcir_lock);

	return (len);
}
#endif /* DRM_LINUX */

static int
drm_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		TUNABLE_INT_FETCH("drm.debug", &drm_debug);
		TUNABLE_INT_FETCH("drm.notyet", &drm_notyet);
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

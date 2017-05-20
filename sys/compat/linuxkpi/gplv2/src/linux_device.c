#include <linux/device.h>
#include <linux/pci.h>

#undef resource

static MALLOC_DEFINE(M_DEVRES, "devres", "Linux compat devres");

static struct devres *
dr_node_alloc(dr_release_t release, size_t size, int flags)
{
	size_t tot_size = sizeof(struct devres) + size;
	struct devres *dr;

	if ((dr = malloc(tot_size, M_DEVRES, flags|M_ZERO)) == NULL)
		return (NULL);
	
	INIT_LIST_HEAD(&dr->node.entry);
	dr->node.release = release;
	return (dr);
}

static void
dr_list_insert(struct device *dev, struct devres_node *node)
{
	list_add_tail(&node->entry, &dev->devres_head);
}

static struct devres *
dr_list_lookup(struct device *dev, dr_release_t release,
	       dr_match_t match, void *match_data)
{
	struct devres_node *node;

	list_for_each_entry_reverse(node, &dev->devres_head, entry) {
		struct devres *dr = container_of(node, struct devres, node);

		if (node->release != release)
			continue;
		if (match && !match(dev, dr->data, match_data))
			continue;
		return (dr);
	}

	return (NULL);
}

void *
devres_alloc_node(dr_release_t release, size_t size, gfp_t gfp, int nid __unused)
{
	struct devres *dr;

	if ((dr = dr_node_alloc(release, size, gfp)) == NULL)
		return (NULL);
	return (dr->data);
}

void
devres_add(struct device *dev, void *res)
{
	struct devres *dr = container_of(res, struct devres, data);
	unsigned long flags;

	spin_lock_irqsave(&dev->devres_lock, flags);
	dr_list_insert(dev, &dr->node);
	spin_unlock_irqrestore(&dev->devres_lock, flags);
}

void *
devres_remove(struct device *dev, dr_release_t release,
	      dr_match_t match, void *match_data)
{
	struct devres *dr;
	unsigned long flags;

	spin_lock_irqsave(&dev->devres_lock, flags);
	dr = dr_list_lookup(dev, release, match, match_data);
	if (dr)
		list_del_init(&dr->node.entry);
	spin_unlock_irqrestore(&dev->devres_lock, flags);
	return (dr ? dr->data : NULL);
}

int
devres_release(struct device *dev, dr_release_t release,
		       dr_match_t match, void *match_data)
{
	void *res;

	res = devres_remove(dev, release, match, match_data);
	if (__predict_false(res == 0))
		return -ENOENT;

	(*release)(dev, res);
	devres_free(res);
	return 0;
}

void
devres_free(void *res)
{
	if (res) {
		struct devres *dr = container_of(res, struct devres, data);

		BUG_ON(!list_empty(&dr->node.entry));
		free(dr, M_DEVRES);
	}
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
		type = pci_resource_type(pdev, bar);
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

#ifndef _LINUX_GPLV2_PCI_H_
#define	_LINUX_GPLV2_PCI_H_

#include_next <linux/pci.h>

static inline bool
pci_is_root_bus(struct pci_bus *pbus)
{

	return (pbus->self == NULL);
}

static inline struct pci_dev *
pci_upstream_bridge(struct pci_dev *dev)
{

	UNIMPLEMENTED();
	return (NULL);
}

static inline void *
pci_platform_rom(struct pci_dev *pdev, size_t *size)
{

	UNIMPLEMENTED();
	return (NULL);
}

static inline void
linux_pci_save_state(struct pci_dev *pdev)
{

	panic("implment me!!");
	UNIMPLEMENTED();
}

static inline void
linux_pci_restore_state(struct pci_dev *pdev)
{

	panic("implment me!!");
	UNIMPLEMENTED();
}

static inline void
pci_ignore_hotplug(struct pci_dev *pdev)
{

	UNIMPLEMENTED();
}

static inline void *
pci_alloc_consistent(struct pci_dev *hwdev, size_t size, dma_addr_t *dma_handle)
{

	return (dma_alloc_coherent(hwdev == NULL ? NULL : &hwdev->dev, size,
	    dma_handle, GFP_ATOMIC));
}

static inline int
pcie_get_readrq(struct pci_dev *dev)
{
	u16 ctl;

	pcie_capability_read_word(dev, PCI_EXP_DEVCTL, &ctl);

	return 128 << ((ctl & PCI_EXP_DEVCTL_READRQ) >> 12);
}

static inline void
pci_resource_to_user(const struct pci_dev *dev, int bar,
		const struct linux_resource *rsrc, resource_size_t *start,
		resource_size_t *end)
{

	*start = rsrc->start;
	*end = rsrc->end;
}
#endif /* _LINUX_GPLV2_PCI_H_ */

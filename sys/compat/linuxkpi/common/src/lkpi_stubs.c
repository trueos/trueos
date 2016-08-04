

#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/threads.h>
#include <linux/cpumask.h>
#include <linux/pci.h>

cpumask_t *cpu_all_mask;
cpumask_t *cpu_online_mask;

struct pci_dev *
linux_pci_get_device(unsigned int vendor, unsigned int device, struct pci_dev *from)
{

	return (NULL);
}
int
pci_probe_reset_function(struct pci_dev *dev)
{
	return (0);
}

int
pci_reset_function(struct pci_dev *dev)
{
	return (0);
}

struct pci_bus
*pci_find_next_bus(const struct pci_bus *from)
{

	return (NULL);
}

void
pci_stop_and_remove_bus_device(struct pci_dev *dev)
{
	/* NO-OP */
}

void
pci_stop_and_remove_bus_device_locked(struct pci_dev *dev)
{
	/* NO-OP */
}

unsigned int
pci_rescan_bus_bridge_resize(struct pci_dev *bridge)
{
	/* NO-OP */
	return (0);
}

unsigned int
pci_rescan_bus(struct pci_bus *bus)
{
	/* NO-OP */
	return (0);
}

void
pci_lock_rescan_remove(void)
{

}

void
pci_unlock_rescan_remove(void)
{
	
}

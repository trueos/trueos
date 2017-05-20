#ifndef _LINUX_IOPORT_H
#define _LINUX_IOPORT_H
#include <linux/types.h>

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/pciio.h>
#include <sys/rman.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pci_private.h>
#include <sys/resourcevar.h>

#include <machine/resource.h>


#include <linux/compiler.h>
#include <linux/types.h>

extern struct linux_resource ioport_resource;
extern struct linux_resource iomem_resource;

struct linux_resource {
	resource_size_t start;
	resource_size_t end;
	const char *name;
	unsigned long flags;
	struct linux_resource *parent, *sibling, *child;
	struct resource *r;
	int rid;
	int type;
	device_t bsddev;
};


struct device;


#define devm_request_region(dev,start,n,name)				\
	__devm_request_region(dev, &ioport_resource, (start), (n), (name))
#define devm_request_mem_region(dev,start,n,name) \
	__devm_request_region(dev, &iomem_resource, (start), (n), (name))

static inline struct linux_resource *
__devm_request_region(struct device *dev,
		      struct linux_resource *parent, resource_size_t start,
		      resource_size_t n, const char *name)
{
	/* XXX return a non-NULL value where the driver just needs to reserve the range */
	return ((void *)0x1);
}

#define IORESOURCE_BITS		0x000000ff	/* Bus-specific bits */

#define IORESOURCE_TYPE_BITS	0x00001f00	/* Resource type */
#define	IORESOURCE_MEM	(1 << SYS_RES_MEMORY)
#define	IORESOURCE_IO	(1 << SYS_RES_IOPORT)
#define	IORESOURCE_IRQ	(1 << SYS_RES_IRQ)
#define IORESOURCE_REG		0x00000300	/* Register offsets */
#define IORESOURCE_DMA		0x00000800
#define IORESOURCE_BUS		0x00001000


/* PCI ROM control bits (IORESOURCE_BITS) */
#define IORESOURCE_ROM_ENABLE		(1<<0)	/* ROM is enabled, same as PCI_ROM_ADDRESS_ENABLE */
#define IORESOURCE_ROM_SHADOW		(1<<1)	/* ROM is copy at C000:0 */
#define IORESOURCE_ROM_COPY		(1<<2)	/* ROM is alloc'd copy, resource field overlaid */
#define IORESOURCE_ROM_BIOS_COPY	(1<<3)	/* ROM is BIOS copy, resource field overlaid */



#define IORESOURCE_EXT_TYPE_BITS 0x01000000	/* Resource extended types */



static inline unsigned long
resource_type(const struct linux_resource *res)
{
	return (res->flags & IORESOURCE_TYPE_BITS);
}
static inline unsigned long
resource_ext_type(const struct linux_resource *res)
{
	return (res->flags & IORESOURCE_EXT_TYPE_BITS);
}

#endif

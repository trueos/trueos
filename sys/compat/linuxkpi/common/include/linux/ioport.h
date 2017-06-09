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

struct device;

#define IORESOURCE_BITS		0x000000ff	/* Bus-specific bits */

#define IORESOURCE_TYPE_BITS	0x00001f00	/* Resource type */
#define	IORESOURCE_MEM	(1 << SYS_RES_MEMORY)
#define	IORESOURCE_IO	(1 << SYS_RES_IOPORT)
#define	IORESOURCE_IRQ	(1 << SYS_RES_IRQ)
#define IORESOURCE_REG		0x00000300	/* Register offsets */
#define IORESOURCE_DMA		0x00000800
#define IORESOURCE_BUS		0x00001000

#endif

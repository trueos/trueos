# ixnvdimm Loadable Kernel Module
#
# $FreeBSD$

.PATH:  ${.CURDIR}/../../dev/ixnvdimm

KMOD=	ixnvdimm
SRCS=	ixnvdimm.c ixnvdimm_copy.S
SRCS+=	opt_acpi.h acpi_if.h device_if.h bus_if.h ntb_if.h opt_ddb.h

SRCS+=	acpi_if.h bus_if.h device_if.h

.include <bsd.kmod.mk>

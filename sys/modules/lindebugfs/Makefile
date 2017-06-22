# $FreeBSD$

.PATH: ${.CURDIR}/../../compat/lindebugfs

KMOD=	debugfs
SRCS=	vnode_if.h \
	device_if.h bus_if.h  pci_if.h \
	lindebugfs.c

CFLAGS+= -I${.CURDIR}/../../compat/linuxkpi/gplv2/include
CFLAGS+= -I${.CURDIR}/../../compat/linuxkpi/common/include

.include <bsd.kmod.mk>

# $FreeBSD$

.PATH: ${.CURDIR}/../../compat/lindebugfs

KMOD=	debugfs
SRCS=	vnode_if.h \
	device_if.h bus_if.h  pci_if.h \
	lindebugfs.c

CFLAGS+= -I${.CURDIR}/../../compat/linuxkpi/common/include
CFLAGS+= -I${.CURDIR}/../../contrib/ck/include

.include <bsd.kmod.mk>

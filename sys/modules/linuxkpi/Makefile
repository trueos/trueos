# $FreeBSD$
.PATH:	${.CURDIR}/../../compat/linuxkpi/common/src

KMOD=	linuxkpi
SRCS=	linux_compat.c \
	linux_dmabuf.c \
	linux_fs.c \
	linux_i2c.c \
	linux_idr.c \
	linux_kmod.c \
	linux_mtrr.c \
	linux_page.c \
	linux_pci.c \
	linux_radix.c \
	linux_reservation.c \
	linux_string.c \
	linux_sysfs.c \
	linux_usb.c

SRCS+=	bus_if.h \
	device_if.h \
	pci_if.h \
	vnode_if.h \
	usb_if.h \
	opt_usb.h

CFLAGS+= -I${.CURDIR}/../../compat/linuxkpi/common/include

.include <bsd.kmod.mk>
CWARNFLAGS += -Wno-pointer-arith
CWARNFLAGS.linux_string.c=	-Wno-cast-qual

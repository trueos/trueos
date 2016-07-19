# $FreeBSD$
.PATH:	${.CURDIR}/../../compat/linuxkpi/common/src
.PATH:	${.CURDIR}/../../compat/linsysfs/

KMOD=	linuxkpi
SRCS=	interval_tree.c \
	linux_acpi.c \
	linux_compat.c \
	linux_dmabuf.c \
	linux_fence.c \
	linux_fs.c \
	linux_firmware.c \
	linux_i2c.c \
	linux_idr.c \
	linux_irq.c \
	linux_kmod.c \
	linux_lock.c \
	linux_malloc.c \
	linux_mtrr.c \
	linux_notifier.c \
	linux_page.c \
	linux_pci.c \
	linux_platform_device.c \
	linux_radix.c \
	linux_rcu.c \
	linux_reservation.c \
	linux_schedule.c \
	linux_seq_file.c \
	linux_string.c \
	linux_sysfs.c \
	linux_usb.c \
	linux_video.c \
	linux_work.c \
	lkpi_kern_mutex.c

SRCS+=	bus_if.h \
	device_if.h \
	pci_if.h \
	vnode_if.h \
	usb_if.h \
	opt_usb.h


SRCS+= linsysfs_init.c \
	linsysfs_extern.c


CFLAGS+= -I${.CURDIR}/../../compat/linuxkpi/common/include
CFLAGS+= -I${.CURDIR}/../../contrib/ck/include

#CFLAGS += -DPRINT_UNIMPLEMENTED
# until stable
CFLAGS+= -O0

.include <bsd.kmod.mk>
CWARNFLAGS 		+= -Wno-pointer-arith
CWARNFLAGS.linux_string.c=	-Wno-cast-qual
CWARNFLAGS.linux_acpi.c=	-Wno-cast-qual

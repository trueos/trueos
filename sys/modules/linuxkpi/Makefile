# $FreeBSD$
.PATH:	${SRCTOP}/sys/compat/linuxkpi/common/src ${SRCTOP}/sys/compat/linsysfs

KMOD=	linuxkpi
SRCS=	interval_tree.c \
	linux_acpi.c \
	linux_compat.c \
	linux_current.c \
	linux_device.c \
	linux_dmabuf.c \
	linux_fence.c \
	linux_fs.c \
	linux_firmware.c \
	linux_hrtimer.c \
	linux_i2c.c \
	linux_idr.c \
	linux_irq.c \
	linux_kmod.c \
	linux_kthread.c \
	linux_lock.c \
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
	linux_slab.c \
	linux_sort.c \
	linux_stubs.c \
	linux_sysfs.c \
	linux_tasklet.c \
	linux_usb.c \
	linux_work.c

SRCS+=	bus_if.h \
	device_if.h \
	pci_if.h \
	vnode_if.h \
	usb_if.h \
	opt_ddb.h \
	opt_ktrace.h \
	opt_param.h \
	opt_sched.h \
	opt_usb.h \
	opt_vm.h

SRCS+= 	linsysfs_init.c \
	linsysfs_extern.c \
	pci-sysfs.c


CFLAGS+= -I${SRCTOP}/sys/compat/linuxkpi/common/include
CFLAGS+= -I${SRCTOP}/sys/contrib/ck/include
CFLAGS+= '-DKBUILD_MODNAME="${KMOD}"'
CFLAGS+= -include ${SRCTOP}/sys/dev/drm/drm_os_config.h

#CFLAGS += -DPRINT_UNIMPLEMENTED
# until stable
#CFLAGS+= -O0

.include <bsd.kmod.mk>
CWARNFLAGS 		+= -Wno-pointer-arith
CWARNFLAGS.linux_acpi.c=	-Wno-cast-qual

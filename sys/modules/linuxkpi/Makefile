# $FreeBSD$
.PATH:	${SRCTOP}/sys/compat/linuxkpi/common/src ${SRCTOP}/sys/compat/linsysfs

KMOD=	linuxkpi
SRCS=	linux_compat.c \
	linux_current.c \
	linux_idr.c \
	linux_kmod.c \
	linux_kthread.c \
	linux_lock.c \
	linux_page.c \
	linux_pci.c \
	linux_radix.c \
	linux_rcu.c \
	linux_schedule.c \
	linux_slab.c \
	linux_sysfs.c \
	linux_tasklet.c \
	linux_usb.c \
	linux_work.c

SRCS+=	bus_if.h \
	device_if.h \
	pci_if.h \
	vnode_if.h \
	usb_if.h \
	opt_acpi.h \
	opt_ddb.h \
	opt_ktrace.h \
	opt_param.h \
	opt_sched.h \
	opt_usb.h \
	opt_vm.h

SRCS+= 	linsysfs_init.c \
	linsysfs_extern.c

CFLAGS+= -I${SRCTOP}/sys/compat/linuxkpi/common/include
CFLAGS+= -I${SRCTOP}/sys/contrib/ck/include
CFLAGS+= '-DKBUILD_MODNAME="${KMOD}"'
CFLAGS+= -include ${SRCTOP}/sys/dev/drm/drm_os_config.h

#CFLAGS += -DPRINT_UNIMPLEMENTED
# until stable
#CFLAGS+= -O0

.include <bsd.kmod.mk>
CWARNFLAGS 		+= -Wno-pointer-arith

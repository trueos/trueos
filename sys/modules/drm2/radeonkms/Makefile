# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/drm2/radeon

KMOD	= radeonkms
SRCS	= \
	atom.c \
	uvd_v1_0.c \
	uvd_v2_2.c \
	uvd_v3_1.c \
	uvd_v4_2.c \
	vce_v1_0.c \
	vce_v2_0.c

CFLAGS+= -I${.CURDIR}/../../../compat/linuxkpi/common/include
CFLAGS+= -I${.CURDIR}/../../../compat/linuxkpi/common/include/drm
CFLAGS+= -I${.CURDIR}/../../../compat/linuxkpi/common/include/uapi
CFLAGS+= -I${.CURDIR}/../../../dev/drm2/radeon
CFLAGS+= '-DKBUILD_MODNAME="${KMOD}"'

SRCS    +=device_if.h vnode_if.h bus_if.h pci_if.h device_if.h iicbus_if.h opt_drm.h \
        opt_vm.h opt_compat.h opt_syscons.h

.include <bsd.kmod.mk>

CWARNFLAGS.vce_v1_0.c=		-Wno-missing-prototypes -Wno-cast-qual
CWARNFLAGS.vce_v2_0.c=		-Wno-missing-prototypes
CWARNFLAGS+=			-Wno-pointer-arith ${CWARNFLAGS.${.IMPSRC:T}}

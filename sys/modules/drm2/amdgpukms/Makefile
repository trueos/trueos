# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/drm2/amd/amdgpu

KMOD	= amdgpukms
SRCS	= \
	uvd_v4_2.h \
	uvd_v4_2.c \
	uvd_v5_0.h \
	uvd_v5_0.c \
	uvd_v6_0.h \
	uvd_v6_0.c \
	vce_v2_0.h \
	vce_v2_0.c \
	vce_v3_0.h \
	vce_v3_0.c \
	vi.h \
	vi.c \
	vi_dpm.h \
	vid.h

CFLAGS+= -I${.CURDIR}/../../../compat/linuxkpi/common/include
CFLAGS+= -I${.CURDIR}/../../../compat/linuxkpi/common/include/drm
CFLAGS+= -I${.CURDIR}/../../../compat/linuxkpi/common/include/uapi
CFLAGS+= -I${.CURDIR}/../../../dev/drm2/amd/amdgpu
CFLAGS+= -I${.CURDIR}/../../../dev/drm2/amd/include
CFLAGS+= -I${.CURDIR}/../../../dev/drm2/amd/powerplay/inc
CFLAGS+= -I${.CURDIR}/../../../dev/drm2/amd/scheduler
CFLAGS+= -I${.CURDIR}/../../../dev/drm2/amd/include/asic_reg
CFLAGS+= '-DKBUILD_MODNAME="${KMOD}"'

SRCS	+=device_if.h vnode_if.h bus_if.h pci_if.h device_if.h iicbus_if.h opt_drm.h \
        opt_vm.h opt_compat.h opt_syscons.h

CWARNFLAGS.r600_cp.c=		-Wno-unused-value
CWARNFLAGS+=			-Wno-pointer-sign ${CWARNFLAGS.${.IMPSRC:T}}

.include <bsd.kmod.mk>

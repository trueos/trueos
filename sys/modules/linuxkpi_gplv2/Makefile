.PATH:	${SRCTOP}/sys/compat/linuxkpi/gplv2/src

KMOD=	linuxkpi_gplv2

SRCS=	linux_kmod_gplv2.c	\
	linux_backlight.c	\
	linux_fs_attr.c		\
	linux_video.c

SRCS+=	bus_if.h		\
	device_if.h		\
	pci_if.h		\
	vnode_if.h

CFLAGS+= -include ${SRCTOP}/sys/dev/drm/drm_os_config.h
CFLAGS+= -I${SRCTOP}/sys/compat/linuxkpi/gplv2/include
CFLAGS+= -I${SRCTOP}/sys/compat/linuxkpi/common/include
CFLAGS+= '-DKBUILD_MODNAME="${KMOD}"'

.include <bsd.kmod.mk>
CWARNFLAGS+=	-Wno-pointer-arith

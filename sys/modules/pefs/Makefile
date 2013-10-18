# $FreeBSD$

.PATH:	${.CURDIR}/../../fs/pefs

KMOD=	pefs
SRCS=	vnode_if.h \
	pefs_subr.c pefs_vfsops.c pefs_vnops.c pefs_xbase64.c pefs_crypto.c \
	pefs_dircache.c \
	pefs_xts.c pefs_hmac.c pefs_pkcs5v2.c vmac.c

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
SRCS+=	pefs_aesni.c
CFLAGS+= -DPEFS_AESNI
.endif

CFLAGS+= -I${.CURDIR}/../../

.include <bsd.kmod.mk>

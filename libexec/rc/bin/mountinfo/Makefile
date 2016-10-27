# $FreeBSD$

PACKAGE=runtime
PROG=	mountinfo
SRCS=	mountinfo.c \
       	${.CURDIR}/../../../../contrib/openrc/rc-misc.c \
       	${.CURDIR}/../../../../contrib/openrc/_usage.c
CFLAGS+=	-I${.CURDIR}/../../../../contrib/openrc \
	       	-I${.CURDIR}/../../../../lib/libeinfo \
	       	-I${.CURDIR}/../../../../lib/librc \
	       	-I${.CURDIR}/../../../../include

WARNS?=	3

BINDIR=/libexec/rc/bin
LIBADD=	einfo kvm rc util
MAN=

.include <bsd.prog.mk>

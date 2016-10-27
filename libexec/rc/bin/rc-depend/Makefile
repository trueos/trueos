# $FreeBSD$

PACKAGE=runtime
PROG=	rc-depend
SRCS=	rc-depend.c \
       	${.CURDIR}/../../../../contrib/openrc/rc-misc.c \
       	${.CURDIR}/../../../../contrib/openrc/_usage.c
CFLAGS+=	-I${.CURDIR}/../../../../contrib/openrc \
	       	-I${.CURDIR}/../../../../lib/libeinfo \
	       	-I${.CURDIR}/../../../../lib/librc \
	       	-I${.CURDIR}/../../../../include

WARNS?=	3

LIBADD=	einfo kvm rc util

.include <bsd.prog.mk>

# $FreeBSD$

PACKAGE=runtime
PROG=	rc-status
SRCS=	rc-status.c \
       	${.CURDIR}/../../contrib/openrc/rc-misc.c \
       	${.CURDIR}/../../contrib/openrc/_usage.c
CFLAGS+=	-I${.CURDIR}/../../contrib/openrc \
	       	-I${.CURDIR}/../../lib/libeinfo \
	       	-I${.CURDIR}/../../lib/librc \
	       	-I${.CURDIR}/../../include

WARNS?=	3
MAN=	rc-status.8

LIBADD=	einfo rc

.include <bsd.prog.mk>

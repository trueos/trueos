# $FreeBSD$

PACKAGE=runtime
PROG=	start-stop-daemon
SRCS=	start-stop-daemon.c \
       	${.CURDIR}/../../contrib/openrc/rc-misc.c \
       	${.CURDIR}/../../contrib/openrc/_usage.c
CFLAGS+=	-I${.CURDIR}/../../contrib/openrc \
	       	-I${.CURDIR}/../../lib/libeinfo \
	       	-I${.CURDIR}/../../lib/librc \
	       	-I${.CURDIR}/../../include

WARNS?=	3
MAN=	start-stop-daemon.8

LIBADD=	einfo rc

.include <bsd.prog.mk>

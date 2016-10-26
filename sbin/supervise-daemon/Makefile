# $FreeBSD$

PACKAGE=runtime
PROG=	supervise-daemon
SRCS=	supervise-daemon.c \
       	${.CURDIR}/../../contrib/openrc/rc-misc.c \
       	${.CURDIR}/../../contrib/openrc/_usage.c
CFLAGS+=	-I${.CURDIR}/../../contrib/openrc \
	       	-I${.CURDIR}/../../lib/libeinfo \
	       	-I${.CURDIR}/../../lib/librc \
	       	-I${.CURDIR}/../../include

WARNS?=	3
MAN=	supervise-daemon.8

LIBADD=	einfo rc

.include <bsd.prog.mk>

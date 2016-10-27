# $FreeBSD$

PACKAGE=runtime
PROG=	rc-abort
SRCS=	rc-abort.c
CFLAGS+=	-I${.CURDIR}/../../../../contrib/openrc \
	       	-I${.CURDIR}/../../../../lib/libeinfo \
	       	-I${.CURDIR}/../../../../lib/librc \
	       	-I${.CURDIR}/../../../../include

WARNS?=	3

LIBADD=	einfo kvm rc util

.include <bsd.prog.mk>

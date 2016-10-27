# $FreeBSD$

PACKAGE=runtime
PROG=	shell_var
SRCS=	shell_var.c
CFLAGS+=	-I${.CURDIR}/../../../../contrib/openrc \
	       	-I${.CURDIR}/../../../../lib/libeinfo \
	       	-I${.CURDIR}/../../../../lib/librc \
	       	-I${.CURDIR}/../../../../include

WARNS?=	3

LIBADD=	einfo kvm rc util

.include <bsd.prog.mk>

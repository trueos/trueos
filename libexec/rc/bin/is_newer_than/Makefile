# $FreeBSD$

PACKAGE=runtime
PROG=	is_newer_than
SRCS=	is_newer_than.c \
       	${.CURDIR}/../../../../contrib/openrc/rc-misc.c
CFLAGS+=	-I${.CURDIR}/../../../../contrib/openrc \
	       	-I${.CURDIR}/../../../../lib/libeinfo \
	       	-I${.CURDIR}/../../../../lib/librc \
	       	-I${.CURDIR}/../../../../include

WARNS?=	3

BINDIR=/libexec/rc/bin
LIBADD=	einfo kvm rc util
MAN=

.include <bsd.prog.mk>

# $FreeBSD$

PACKAGE=runtime
PROG=	get_options
LINKS=  ${BINDIR}/get_options ${BINDIR}/service_get_value \
	${BINDIR}/get_options ${BINDIR}/service_set_value \
	${BINDIR}/get_options ${BINDIR}/save_options
SRCS=	do_value.c \
       	${.CURDIR}/../../../../contrib/openrc/rc-misc.c
CFLAGS+=	-I${.CURDIR}/../../../../contrib/openrc \
	       	-I${.CURDIR}/../../../../lib/libeinfo \
	       	-I${.CURDIR}/../../../../lib/librc \
	       	-I${.CURDIR}/../../../../include 

WARNS?=	3

LIBADD=	einfo kvm rc util
MAN=

.include <bsd.prog.mk>

# $FreeBSD$

PACKAGE=runtime
PROG=	service_starting
LINKS=  ${BINDIR}/service_starting ${BINDIR}/service_started \
	${BINDIR}/service_starting ${BINDIR}/service_stopping \
	${BINDIR}/service_starting ${BINDIR}/service_stopped \
	${BINDIR}/service_starting ${BINDIR}/service_inactive \
	${BINDIR}/service_starting ${BINDIR}/service_wasinactive \
	${BINDIR}/service_starting ${BINDIR}/service_hotplugged \
	${BINDIR}/service_starting ${BINDIR}/service_started_daemon \
	${BINDIR}/service_starting ${BINDIR}/service_crashed
SRCS=	do_service.c \
       	${.CURDIR}/../../../../contrib/openrc/rc-misc.c
CFLAGS+=	-I${.CURDIR}/../../../../contrib/openrc \
	       	-I${.CURDIR}/../../../../lib/libeinfo \
	       	-I${.CURDIR}/../../../../lib/librc \
	       	-I${.CURDIR}/../../../../include 

WARNS?=	3

LIBADD=	einfo kvm rc util
MAN=

.include <bsd.prog.mk>

# $FreeBSD$

PACKAGE=runtime
PROG=	mark_service_starting
LINKS=  ${BINDIR}/mark_service_starting ${BINDIR}/mark_service_started \
	${BINDIR}/mark_service_starting ${BINDIR}/mark_service_stopping \
	${BINDIR}/mark_service_starting ${BINDIR}/mark_service_stopped \
	${BINDIR}/mark_service_starting ${BINDIR}/mark_service_inactive \
	${BINDIR}/mark_service_starting ${BINDIR}/mark_service_wasinactive \
	${BINDIR}/mark_service_starting ${BINDIR}/mark_service_hotplugged \
	${BINDIR}/mark_service_starting ${BINDIR}/mark_service_failed
SRCS=	do_mark_service.c \
       	${.CURDIR}/../../../../contrib/openrc/rc-misc.c
CFLAGS+=	-I${.CURDIR}/../../../../contrib/openrc \
	       	-I${.CURDIR}/../../../../lib/libeinfo \
	       	-I${.CURDIR}/../../../../lib/librc \
	       	-I${.CURDIR}/../../../../include 

WARNS?=	3

LIBADD=	einfo kvm rc util

.include <bsd.prog.mk>

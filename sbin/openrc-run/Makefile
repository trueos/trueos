# $FreeBSD$

PACKAGE=runtime
PROG=	openrc-run
LINKS=  ${BINDIR}/openrc-run ${BINDIR}/runscript
SRCS=	openrc-run.c \
       	${.CURDIR}/../../contrib/openrc/rc-misc.c \
       	${.CURDIR}/../../contrib/openrc/rc-plugin.c \
       	${.CURDIR}/../../contrib/openrc/_usage.c
CFLAGS+=	-I${.CURDIR}/../../contrib/openrc \
	       	-I${.CURDIR}/../../lib/libeinfo \
	       	-I${.CURDIR}/../../lib/librc

WARNS?=	3
MAN=	openrc-run.8

LIBADD=	einfo rc

.include <bsd.prog.mk>

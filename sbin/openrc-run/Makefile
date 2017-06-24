# $FreeBSD$

PACKAGE=runtime
PROG=	openrc-run
LINKS=  ${BINDIR}/openrc-run ${BINDIR}/runscript
SRCS=	${.CURDIR}/../../contrib/openrc/src/rc/openrc-run.c \
	${.CURDIR}/../../contrib/openrc/src/rc/rc-misc.c \
	${.CURDIR}/../../contrib/openrc/src/rc/rc-plugin.c \
	${.CURDIR}/../../contrib/openrc/src/rc/_usage.c
CFLAGS+=	-I${.CURDIR}/../../contrib/openrc/src/rc \
		-I${.CURDIR}/../../contrib/openrc/src/libeinfo \
		-I${.CURDIR}/../../contrib/openrc/src/librc \
                -I${.OBJDIR}/../../lib/librc/contrib/openrc/src/librc \
		-I${.CURDIR}/../../contrib/openrc/src/includes

WARNS?=	3
MAN=	${.CURDIR}/../../contrib/openrc/man/openrc-run.8

LIBADD=	einfo rc util

.include <bsd.prog.mk>

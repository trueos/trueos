# $FreeBSD$

PACKAGE=runtime
PROG=	einfo
LINKS=  ${BINDIR}/einfo ${BINDIR}/einfon \
	${BINDIR}/einfo ${BINDIR}/ewarn \
	${BINDIR}/einfo ${BINDIR}/ewarnn \
	${BINDIR}/einfo ${BINDIR}/eerror \
	${BINDIR}/einfo ${BINDIR}/eerrorn \
	${BINDIR}/einfo ${BINDIR}/ebegin \
	${BINDIR}/einfo ${BINDIR}/eend \
	${BINDIR}/einfo ${BINDIR}/ewend \
	${BINDIR}/einfo ${BINDIR}/eindent \
	${BINDIR}/einfo ${BINDIR}/eoutdent \
	${BINDIR}/einfo ${BINDIR}/esyslog \
	${BINDIR}/einfo ${BINDIR}/eval_ecolors \
	${BINDIR}/einfo ${BINDIR}/ewaitfile \
	${BINDIR}/einfo ${BINDIR}/veinfo \
	${BINDIR}/einfo ${BINDIR}/vewarn \
	${BINDIR}/einfo ${BINDIR}/vebegin \
	${BINDIR}/einfo ${BINDIR}/veend \
	${BINDIR}/einfo ${BINDIR}/vewend \
	${BINDIR}/einfo ${BINDIR}/veindent \
	${BINDIR}/einfo ${BINDIR}/veoutdent
SRCS=	do_e.c \
       	${.CURDIR}/../../../../contrib/openrc/rc-misc.c
CFLAGS+=	-I${.CURDIR}/../../../../contrib/openrc \
	       	-I${.CURDIR}/../../../../lib/libeinfo \
	       	-I${.CURDIR}/../../../../lib/librc \
	       	-I${.CURDIR}/../../../../include 

WARNS?=	3

LIBADD=	einfo kvm rc util

.include <bsd.prog.mk>

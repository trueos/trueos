# $FreeBSD$

PACKAGE=runtime
PROG=	rexec

.include <src.opts.mk>

ORSRC=	${SRCTOP}/contrib/poudriere/src/libexec/poudriere
EXTSRC=	${SRCTOP}/contrib/poudriere/external

.PATH:  ${ORSRC}/rexec

SRCS=	rexec.c \
	${EXTSRC}/libnv/sys/dnvlist.c \
	${EXTSRC}/libnv/msgio.c \
	${EXTSRC}/libnv/sys/nvlist.c \
	${EXTSRC}/libnv/sys/nvpair.c
CFLAGS+=	-I${SRCTOP}/libexec/poudriere \
		-I${EXTSRC}/libnv/sys \
		-I${EXTSRC}/libnv

WARNS?=	1

BINDIR=/libexec/poudriere
LIBADD=
MAN=

.include <bsd.prog.mk>

# $FreeBSD$

PACKAGE=runtime
PROG=	cached

.include <src.opts.mk>

ORSRC=	${SRCTOP}/contrib/poudriere/src/libexec/poudriere
EXTSRC=	${SRCTOP}/contrib/poudriere/external

.PATH:  ${ORSRC}/cached

SRCS=	cached.c
CFLAGS+=	-I${SRCTOP}/libexec/poudriere \
		-I${EXTSRC}/klib

WARNS?=	1

BINDIR=/libexec/poudriere
LIBADD=	util rt
MAN=

.include <bsd.prog.mk>

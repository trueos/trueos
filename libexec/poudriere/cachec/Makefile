# $FreeBSD$

PACKAGE=runtime
PROG=	cachec

.include <src.opts.mk>

ORSRC=    ${SRCTOP}/contrib/poudriere/src/libexec/poudriere

.PATH:  ${ORSRC}/cachec

SRCS=	cachec.c
CFLAGS+=	-I${SRCTOP}/libexec/poudriere

WARNS?=	3

BINDIR=/libexec/poudriere
LIBADD=	rt
MAN=

.include <bsd.prog.mk>

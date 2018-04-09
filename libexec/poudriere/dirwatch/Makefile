# $FreeBSD$

PACKAGE=runtime
PROG=	dirwatch

.include <src.opts.mk>

ORSRC=	${SRCTOP}/contrib/poudriere/src/libexec/poudriere
EXTSRC=	${SRCTOP}/contrib/poudriere/external

.PATH:  ${ORSRC}/dirwatch

SRCS=	dirwatch.c
CFLAGS+=	-I${SRCTOP}/libexec/poudriere

WARNS?=	1

BINDIR=/libexec/poudriere
LIBADD=
MAN=

.include <bsd.prog.mk>

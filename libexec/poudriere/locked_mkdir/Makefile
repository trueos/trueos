# $FreeBSD$

PACKAGE=runtime
PROG=	locked_mkdir

.include <src.opts.mk>

ORSRC=	${SRCTOP}/contrib/poudriere/src/libexec/poudriere
EXTSRC=	${SRCTOP}/contrib/poudriere/external

.PATH:  ${ORSRC}/locked_mkdir

SRCS=	locked_mkdir.c
CFLAGS+=	-I${SRCTOP}/libexec/poudriere

WARNS?=	1

BINDIR=/libexec/poudriere
LIBADD=
MAN=

.include <bsd.prog.mk>

# $FreeBSD$

PACKAGE=runtime
PROG=	cpdup

.include <src.opts.mk>

ORSRC=	${SRCTOP}/contrib/poudriere/src/libexec/poudriere
EXTSRC=	${SRCTOP}/contrib/poudriere/external

.PATH:  ${ORSRC}/cpdup

SRCS=	cpdup.c hcproto.c hclink.c misc.c fsmid.c md5.c
CFLAGS+=	-I${SRCTOP}/libexec/poudriere \
		-D_ST_FLAGS_PRESENT_=1

WARNS?=	1

BINDIR=/libexec/poudriere
LIBADD= md
MAN=

.include <bsd.prog.mk>

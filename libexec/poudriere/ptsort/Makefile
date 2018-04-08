# $FreeBSD$

PACKAGE=runtime
PROG=	ptsort

.include <src.opts.mk>

ORSRC=	${SRCTOP}/contrib/poudriere/external
EXTSRC=	${SRCTOP}/contrib/poudriere/external

.PATH:  ${ORSRC}/ptsort/bin

SRCS=	ptsort.c \
	${EXTSRC}/ptsort/lib/aa_tree.c \
	${EXTSRC}/ptsort/lib/fline.c
CFLAGS+=	-I${SRCTOP}/libexec/poudriere \
		-I${EXTSRC}/ptsort/lib

WARNS?=	1

BINDIR=/libexec/poudriere
LIBADD=
MAN=

.include <bsd.prog.mk>

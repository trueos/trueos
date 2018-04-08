# $FreeBSD$

PACKAGE=runtime
PROG=	poudriered

.include <src.opts.mk>

ORSRC=	${SRCTOP}/contrib/poudriere/src
EXTSRC=	${SRCTOP}/contrib/poudriere/external

.PATH:  ${ORSRC}/poudriered

SRCS=	poudriered.c \
	${EXTSRC}/libucl/src/ucl_emitter.c \
	${EXTSRC}/libucl/src/ucl_emitter_streamline.c \
	${EXTSRC}/libucl/src/ucl_emitter_utils.c \
	${EXTSRC}/libucl/src/ucl_hash.c \
	${EXTSRC}/libucl/src/ucl_parser.c \
	${EXTSRC}/libucl/src/ucl_util.c \
	${EXTSRC}/libucl/src/xxhash.c
CFLAGS+=	-I${SRCTOP}/libexec/poudriere \
		-I${EXTSRC}/libucl/klib \
		-I${EXTSRC}/libucl/include \
		-I${EXTSRC}/libucl/src \
		-I${EXTSRC}/libucl/uthash \
		-DPREFIX=""

WARNS?=	1

BINDIR=/libexec/poudriere
LIBADD= sbuf util
MAN=

.include <bsd.prog.mk>

# $FreeBSD$

PACKAGE=lib${LIB}
LIB=		einfo
SHLIB_MAJOR=	1
SHLIB_MINOR=	0
SRCS=		libeinfo.c
INCS=		einfo.h helpers.h hidden-visibility.h
CFLAGS+=	-D_GNU_SOURCE
.if defined(HAVE_DEBUG)
CFLAGS+=	-g
CFLAGS+=	-DHAVE_DEBUG
.endif

SYM_MAPS+=      ${.CURDIR}/einfo.map
SYMBOL_MAPS=    ${SYM_MAPS}

MLINKS=

.include <bsd.lib.mk>

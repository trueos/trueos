# $FreeBSD$

PACKAGE=lib${LIB}
LIB=		rc
SHLIBDIR?=	/lib
SHLIB_MAJOR=	1
SHLIB_MINOR=	0
SRCS=		librc.c librc-daemon.c librc-depend.c librc-misc.c librc-stringlist.c
INCS=		librc.h queue.h ../../include/rc.h rc-misc.h
CFLAGS+=	-D_GNU_SOURCE -I${.CURDIR}/../libeinfo -I${.CURDIR}/../../include
.if defined(HAVE_DEBUG)
CFLAGS+=	-g
CFLAGS+=	-DHAVE_DEBUG
.endif

SYM_MAPS+=      ${.CURDIR}/rc.map
SYMBOL_MAPS=    ${SYM_MAPS}

MLINKS=

.include <bsd.lib.mk>

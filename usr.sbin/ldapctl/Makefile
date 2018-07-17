#	$OpenBSD: Makefile,v 1.9 2017/07/03 22:21:47 espie Exp $

.PATH:		${.CURDIR}/../ldapd

PROG=		ldapctl
MAN=		ldapctl.8
SRCS=		ldapctl.c parse.y btree.c log.c logmsg.c ber.c util.c \
		index.c attributes.c schema.c syntax.c matching.c

LIBADD=		openbsd event tls crypto ssl z util crypt

CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../ldapd
CFLAGS+=	-I${SRCTOP}/contrib/pf/libevent
CFLAGS+=	-I${SRCTOP}/lib/libopenbsd

WARNS=	2

.include <bsd.prog.mk>

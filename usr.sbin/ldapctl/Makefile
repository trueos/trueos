#	$OpenBSD: Makefile,v 1.6 2016/05/01 10:40:27 tb Exp $

.PATH:		${.CURDIR}/../ldapd

PROG=		ldapctl

MAN=		ldapctl.8
SRCS=		ldapctl.c parse.y btree.c log.c ber.c util.c \
		index.c attributes.c schema.c syntax.c matching.c

LIBADD=		openbsd event tls crypto ssl z util crypt

CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../ldapd
CFLAGS+=	-I${SRCTOP}/contrib/pf/libevent
CFLAGS+=	-I${SRCTOP}/lib/libopenbsd

WARNS=	2

.include <bsd.prog.mk>

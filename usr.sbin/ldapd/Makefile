#	$OpenBSD: Makefile,v 1.13 2016/05/01 00:32:37 jmatthew Exp $
#	$FreeBSD$

PROG=		ldapd
SRCS=		ber.c log.c control.c \
		util.c ldapd.c ldape.c conn.c attributes.c namespace.c \
		btree.c filter.c search.c parse.y \
		auth.c modify.c index.c evbuffer_tls.c \
		validate.c uuid.c schema.c imsgev.c syntax.c matching.c

MAN=		ldapd.8 ldapd.conf.5

LIBADD=		openbsd event tls crypto ssl z util crypt

CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${SRCTOP}/contrib/pf/libevent
CFLAGS+=	-I${SRCTOP}/lib/libopenbsd

CLEANFILES+=	y.tab.h parse.c

WARNS=	2

.include <bsd.prog.mk>

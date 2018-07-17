#	$OpenBSD: Makefile,v 1.15 2017/01/20 11:55:08 benno Exp $

PROG=		ldapd
MAN=		ldapd.8 ldapd.conf.5
SRCS=		ber.c log.c logmsg.c control.c \
		util.c ldapd.c ldape.c conn.c attributes.c namespace.c \
		btree.c filter.c search.c parse.y \
		auth.c modify.c index.c evbuffer_tls.c \
		validate.c uuid.c schema.c imsgev.c syntax.c matching.c

LIBADD=		openbsd event tls crypto ssl z util crypt

CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${SRCTOP}/contrib/pf/libevent
CFLAGS+=	-I${SRCTOP}/lib/libopenbsd

CLEANFILES+=	y.tab.h parse.c

WARNS=	2

.include <bsd.prog.mk>

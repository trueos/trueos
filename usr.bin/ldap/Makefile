PROG=		ldap
SRCS=		ldapclient.c aldap.c ber.c log.c

LIBADD=		openbsd event tls crypto ssl z util crypt

CFLAGS+=        -I${.CURDIR}
CFLAGS+=        -I${SRCTOP}/contrib/pf/libevent
CFLAGS+=        -I${SRCTOP}/lib/libopenbsd

WARNS=	2

.include <bsd.prog.mk>

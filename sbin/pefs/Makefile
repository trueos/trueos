# $FreeBSD$

SYS=	${.CURDIR}/../../sys
.PATH:	${SYS}/fs/pefs
.PATH:	${SYS}/crypto/rijndael ${SYS}/crypto/sha2

PROG=	pefs
SRCS=	pefs_ctl.c pefs_key.c pefs_keychain.c pefs_subr.c
SRCS+=	pefs_hmac.c
SRCS+=	pefs_pkcs5v2.c
SRCS+=	rijndael-api.c rijndael-api-fst.c rijndael-alg-fst.c
SRCS+=	sha2.c

MAN=	pefs.8

CFLAGS+=-I${SYS}
WARNS?=	2

DPADD=  ${LIBUTIL}
LDADD=  -lutil

BINDIR?= /sbin
.include <bsd.prog.mk>

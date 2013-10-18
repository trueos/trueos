# PAM module for pefs
# $FreeBSD$

SYS=		${.CURDIR}/../../../../sys
PEFSDIR=	${.CURDIR}/../../../../sbin/pefs

LIB=	pam_pefs
MAN=	pam_pefs.8
SRCS=	pam_pefs.c
SRCS+=	pefs_key.c pefs_keychain.c pefs_subr.c
SRCS+=	pefs_hmac.c
SRCS+=	pefs_pkcs5v2.c
SRCS+=	rijndael-api.c rijndael-api-fst.c rijndael-alg-fst.c
SRCS+=	sha2.c

CFLAGS+= -I${PEFSDIR}
CFLAGS+= -I${SYS}
WARNS?=	2

DPADD= ${LIBUTIL}
LDADD= -lutil

.include <bsd.lib.mk>

.PATH: ${PEFSDIR}
.PATH: ${SYS}/fs/pefs
.PATH: ${SYS}/crypto/rijndael
.PATH: ${SYS}/crypto/sha2

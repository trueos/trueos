# $FreeBSD$

.include <src.opts.mk>

SUBDIR= 
.if ${MK_OPENSSL} != "no"
SUBDIR+=libcrypto libssl
.if ${MK_OPENSSH} != "no"
SUBDIR+=libssh
.endif
.endif

.if ${MK_LIBRESSL} != "no"
SUBDIR+=libtls
.endif

SUBDIR.${MK_TESTS}+= tests

.include <bsd.subdir.mk>

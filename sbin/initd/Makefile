# Modified pathnames.h to allow openrc to run
# This allows switching between rc.d and openrc

PACKAGE=openrc
PROG=	initd
SRCS=	initd.c getmntopts.c
MAN=    initd.8
PRECIOUSPROG=
INSTALLFLAGS=-b -B.bak
CFLAGS+=-DDEBUGSHELL -DSECURE -DLOGIN_CAP -DCOMPAT_SYSV_INIT
LIBADD=	util crypt

# Needed for getmntopts.c
MOUNT=	${SRCTOP}/sbin/mount
CFLAGS+=-I${MOUNT}
.PATH:	${MOUNT}

NO_SHARED?=	YES

.include <bsd.prog.mk>

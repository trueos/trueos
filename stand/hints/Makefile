# $FreeBSD$

.if exists(${SRCTOP}/sys/${MACHINE}/conf/GENERIC.hints)
LINKS=	/boot/GENERIC.hints /boot/device.hints
FILES+=	${SRCTOP}/sys/${MACHINE}/conf/GENERIC.hints
FILESDIR=	/boot
FILESMODE=	444
.endif

.include <bsd.prog.mk>

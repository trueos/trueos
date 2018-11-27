# $FreeBSD$

install:
.if exists(${SRCTOP}/sys/${MACHINE}/conf/GENERIC.hints)
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 444 \
		${SRCTOP}/sys/${MACHINE}/conf/GENERIC.hints \
		${DESTDIR}/boot/device.hints
.endif

.include <bsd.prog.mk>

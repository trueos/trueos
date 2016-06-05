# $FreeBSD$
#
# Common rules for building firmware.  Note this gets auto-included
# by the subdir Makefile's as a consequence of included bsd.kmod.mk.

_FIRM=	${IMG}.bin

CLEANFILES+=	${_FIRM}

FIRMWS=	${_FIRM}:${NAME}

#
# Note that a license ack is not needed for amdgpu 
#
#FIRMWARE_LICENSE=

${_FIRM}: ${.CURDIR}/../../../../contrib/dev/drm2/amdgpukmsfw/${_FIRM}.uu
	uudecode -p $? > ${.TARGET}

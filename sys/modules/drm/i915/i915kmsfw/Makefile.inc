# $FreeBSD$
#
# Common rules for building firmware.  Note this gets auto-included
# by the subdir Makefile's as a consequence of included bsd.kmod.mk.

_FIRM=	${IMG}.bin

CLEANFILES+=	${_FIRM} ${_FIRM}.fwo ${_FIRM}.ko.debug ${_FIRM}.ko.full

FIRMWS=	${_FIRM}:${NAME}

#
# Note that a license ack is not needed for iwn.
#
#FIRMWARE_LICENSE=

${_FIRM}: ${SRCTOP}/sys/contrib/dev/drm/i915kmsfw/${_FIRM}.uu
	uudecode -p $? > ${.TARGET}

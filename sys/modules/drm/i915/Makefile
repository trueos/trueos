# $FreeBSD$

SYSDIR?=${.CURDIR}/../../..
.include "${SYSDIR}/conf/kern.opts.mk"

.if ${MACHINE_CPUARCH} == "amd64"
_i915kms=	i915kms
_i915kmsfw=	i915kmsfw
.endif

SUBDIR = \
        ${_i915kms} \
        ${_i915kmsfw}


.include <bsd.subdir.mk>

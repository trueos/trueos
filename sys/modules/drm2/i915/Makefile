# $FreeBSD$

SYSDIR?=${.CURDIR}/../../..
.include "${SYSDIR}/conf/kern.opts.mk"

.if ${MACHINE_CPUARCH} == "amd64"
_i915kms=	i915kms
_sklguc=	sklguc
_skldmc=	skldmc
.endif

SUBDIR = \
        ${_i915kms} \
	${_sklguc} \
	${_skldmc}

.include <bsd.subdir.mk>

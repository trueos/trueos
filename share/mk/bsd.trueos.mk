# $FreeBSD$

.if !defined(TRUEOS_MK_OPTS)
.if exists(${.CURDIR}/release/release-trueos.sh) || exists(${SRCTOP}/release/release-trueos.sh)

# Check if TRUEOS_MANIFEST is set, if not use the default
.if exists(${.CURDIR}/release)
TRUEOS_RELDIR=	${.CURDIR}/release
.elif exists(${SRCTOP}/release)
TRUEOS_RELDIR=	${SRCTOP}/release
.endif

.if !defined(TRUEOS_RELDIR)
.error "Unable to locate release/ in ${.CURDIR} ${SRCTOP}"
.endif

.if !defined(TRUEOS_MANIFEST)
TRUEOS_MANIFEST=	${TRUEOS_RELDIR}/manifests/freenas-master.json
.endif

# Confirm the file TRUEOS_MANIFEST exists
.if !exists(${TRUEOS_MANIFEST})
.error "Missing file TRUEOS_MANIFEST: ${TRUEOS_MANIFEST}"
.endif

TRUEOS_MK_OPTS=	YES

.endif
.endif

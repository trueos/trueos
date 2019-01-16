# $FreeBSD$

.if !defined(TRUEOS_MK_OPTS)
.if exists(${.CURDIR}/release/release-trueos.sh) || exists(${SRCTOP}/release/release-trueos.sh)

# Check if TRUEOS_MANIFEST is set, if not use the default
.if exists(${.CURDIR}/release/release-trueos.sh)
TRUEOS_RELDIR=	${.CURDIR}/release
.elif exists(${SRCTOP}/release/release-trueos.sh)
TRUEOS_RELDIR=	${SRCTOP}/release
.endif

.if !defined(TRUEOS_RELDIR)
.error "Unable to locate release/trueos-release.sh in ${.CURDIR} ${SRCTOP}"
.endif

.if !defined(TRUEOS_MANIFEST)
TRUEOS_MANIFEST=	${TRUEOS_RELDIR}/trueos-manifest.json
.endif

# Confirm the file TRUEOS_MANIFEST exists
.if !exists(${TRUEOS_MANIFEST})
.error "Missing file TRUEOS_MANIFEST: ${TRUEOS_MANIFEST}"
.endif

# Validate the version of this manifest and sanity check environment
.if make(buildworld) || make(buildkernel) || make(packages)
TM_VERCHECK!=	 (env TRUEOS_MANIFEST=${TRUEOS_MANIFEST} ${TRUEOS_RELDIR}/release-trueos.sh check >&2 ; echo $$?)
.if ${TM_VERCHECK} != "0"
.error Failed environment sanity check!
.endif
.endif

# Set any world/kernel flags from our manifest
TO_WFLAGS!=		/usr/local/bin/jq -r '."base-packages"."world-flags"' \
				${TRUEOS_MANIFEST}
.if ${TO_WFLAGS} != "null"
TO_WFLAGS!=	 (env TRUEOS_MANIFEST=${TRUEOS_MANIFEST} ${TRUEOS_RELDIR}/release-trueos.sh world_flags /tmp/.wflags.${.MAKE.PID})
.include "/tmp/.wflags.${.MAKE.PID}"
.endif
TO_KFLAGS!=		/usr/local/bin/jq -r '."base-packages"."kernel-flags"' \
				${TRUEOS_MANIFEST}
.if ${TO_KFLAGS} != "null"
TO_KFLAGS!=	 (env TRUEOS_MANIFEST=${TRUEOS_MANIFEST} ${TRUEOS_RELDIR}/release-trueos.sh kernel_flags /tmp/.kflags.${.MAKE.PID})
.include "/tmp/.kflags.${.MAKE.PID}"
.endif

TRUEOS_MK_OPTS=	YES

.endif
.endif

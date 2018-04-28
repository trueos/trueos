#!/bin/sh
#-
# Copyright (c) 2018 Kris Moore
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#

export PATH="/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin"

POUDRIERE_BASEFS=${POUDRIERE_BASEFS:-/usr/local/poudriere}
POUDRIERE_BASE=${POUDRIERE_BASE:-trueos-mk-base}
POUDRIERE_PORTS=${POUDRIERE_PORTS:-trueos-mk-ports}

POUDRIERE_PORTDIR="${POUDRIERE_BASEFS}/ports/${POUDRIERE_PORTS}"
POUDRIERE_PKGDIR="${POUDRIERE_BASEFS}/data/packages/${POUDRIERE_BASE}-${POUDRIERE_PORTS}"

exit_err()
{
	echo "ERROR: $1"
	if [ -n "$2" ] ; then
		exit $2
	else
		exit 1
	fi
}

env_check()
{
	if [ -z "$TRUEOS_MANIFEST" ] ; then
		exit_err "Unset TRUEOS_MANIFEST"
	fi
	GH_PORTS=$(jq -r '."ports-url"' $TRUEOS_MANIFEST)
	GH_PORTS_BRANCH=$(jq -r '."ports-branch"' $TRUEOS_MANIFEST)

	if [ ! -d '/usr/ports/distfiles' ] ; then
		mkdir /usr/ports/distfiles
	fi
}

setup_poudriere_conf()
{
	ZPOOL=$(mount | grep 'on / ' | cut -d '/' -f 1)

	# Copy the systems poudriere.conf over
	cat ${SRCDIR}/etc/poudriere.conf \
		| grep -v "ZPOOL=" \
		| grep -v "FREEBSD_HOST=" \
		| grep -v "GIT_PORTSURL=" \
		| grep -v "USE_TMPFS=" \
		| grep -v "BASEFS=" \
		> ${OBJDIR}/poudriere.conf
	echo "Using zpool: $ZPOOL"
	echo "ZPOOL=$ZPOOL" >> ${OBJDIR}/poudriere.conf
	echo "Using Dist Directory: $DIST_DIR"
	echo "FREEBSD_HOST=file://${DIST_DIR}" >> ${OBJDIR}/poudriere.conf
	echo "Using Ports Tree: $GH_PORTS"
	echo "GIT_URL=${GH_PORTS}" >> ${OBJDIR}/poudriere.conf
	echo "USE_TMPFS=data" >> ${OBJDIR}/poudriere.conf
	echo "BASEFS=$POUDRIERE_BASEFS" >> ${OBJDIR}/poudriere.conf
}

setup_poudriere_jail()
{
	# Create new jail
	poudriere -e ${OBJDIR} jail -c -j $POUDRIERE_BASE -m url=file://${DIST_DIR} -v ${OSRELEASE}
	if [ $? -ne 0 ] ; then
		exit_err "Failed creating poudriere jail"
	fi

	# Create the new ports tree
	poudriere -e ${OBJDIR} ports -c -p $POUDRIERE_PORTS -m git -B $GH_PORTS_BRANCH
	if [ $? -ne 0 ] ; then
		exit_err "Failed creating poudriere ports"
	fi

	# Save the list of build flags
	jq -r '."ports-conf" | join("\n")' ${TRUEOS_MANIFEST} >/etc/poudriere.d/${POUDRIERE_BASE}-make.conf
}

build_poudriere()
{
	sleep 60
	if [ $(jq -r '."package-all"' ${TRUEOS_MANIFEST}) = "true" ] ; then
		# Start the build
		poudriere -e ${OBJDIR} bulk -a -j $POUDRIERE_BASE -p ${POUDRIERE_PORTS}
		check_essential_pkgs
	else
		jq -r '."packages" | join("\n")' ${TRUEOS_MANIFEST} > ${OBJDIR}/trueos-mk-bulk-list

		# Start the build
		poudriere -e ${OBJDIR} bulk -f ${OBJDIR}/trueos-mk-bulk-list -j $POUDRIERE_BASE -p ${POUDRIERE_PORTS}
		if [ $? -ne 0 ] ; then
			exit_err "Failed poudriere build"
		fi
		check_essential_pkgs
	fi
}

clean_poudriere()
{
	# Kill previous jail
	poudriere -e ${OBJDIR} jail -k -j $POUDRIERE_BASE -p ${POUDRIERE_PORTS}

	# Delete previous jail
	echo "poudriere -e ${OBJDIR} jail -d -j ${POUDRIERE_BASE}"
	echo -e "y\n" | poudriere -e ${OBJDIR} jail -d -j ${POUDRIERE_BASE}

	# Delete previous ports tree
	echo -e "y\n" | poudriere -e ${OBJDIR} ports -d -p ${POUDRIERE_PORTS}
}

check_essential_pkgs()
{
	if [ "$(jq -r '."essential-packages"' ${TRUEOS_MANIFEST})" = "null" ] ; then
		echo "No essential-packages defined. Skipping..."
		return 0
	fi

	echo "Checking essential-packages..."
	local haveWarn=0

	for i in $(jq -r '."essential-packages" | join(" ")' ${TRUEOS_MANIFEST})
	do

		if [ ! -d "${POUDRIERE_PORTDIR}/${i}" ] ; then
			echo "Invalid PORT: $i"
			continue
		fi

		# Get the pkgname
		unset pkgName
		pkgName=$(make -C ${POUDRIERE_PORTDIR}/${i} -V PKGNAME PORTSDIR=${POUDRIERE_PORTDIR} __MAKE_CONF=${OBJDIR}/poudriere.d/${POUDRIERE_BASE}-${POUDRIERE_PORTS}-make.conf)
		if [ -z "${pkgName}" ] ; then
			echo "Could not get PKGNAME for ${i}"
			haveWarn=1
		fi

		if [ ! -e "${POUDRIERE_PKGDIR}/All/${pkgName}.txz" ] ; then
			echo "Checked: ${POUDRIERE_PKGDIR}/All/${pkgName}.txz"
			echo "WARNING: Missing package ${pkgName} for port ${i}"
			haveWarn=1
		else
			echo "Verified: ${pkgName}"
		fi
   done

   return $haveWarn
}

clean_jails()
{
	setup_poudriere_conf
	clean_poudriere
}

run_poudriere()
{
	clean_jails
	setup_poudriere_jail
	build_poudriere
}

build_iso()
{

}

env_check

case $1 in
	clean) clean_jails ; exit 0 ;;
	poudriere) run_poudriere ;;
	iso) build_iso "$2" ;;
	*) echo "Unknown option selected" ;;
esac

exit 0

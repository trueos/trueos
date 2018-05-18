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
PKG_CMD=${PKG_CMD:-pkg-static}

POUDRIERE_PORTDIR="${POUDRIERE_BASEFS}/ports/${POUDRIERE_PORTS}"
POUDRIERE_PKGDIR="${POUDRIERE_BASEFS}/data/packages/${POUDRIERE_BASE}-${POUDRIERE_PORTS}"
POUDRIERED_DIR_=/etc/poudriere.d

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
	_pdconf="${POUDRIERED_DIR}/${POUDRIERE_BASE}-poudriere.conf"

	if [ ! -d "${POUDRIERED_DIR}" ] ; then
		mkdir -p ${POUDRIERED_DIR}
	fi

	# Copy the systems poudriere.conf over
	cat ${SRCDIR}/etc/poudriere.conf \
		| grep -v "ZPOOL=" \
		| grep -v "FREEBSD_HOST=" \
		| grep -v "GIT_PORTSURL=" \
		| grep -v "USE_TMPFS=" \
		| grep -v "BASEFS=" \
		> ${_pdconf}
	echo "Using zpool: $ZPOOL"
	echo "ZPOOL=$ZPOOL" >> ${_pdconf}
	echo "Using Dist Directory: $DIST_DIR"
	echo "FREEBSD_HOST=file://${DIST_DIR}" >> ${_pdconf}
	echo "Using Ports Tree: $GH_PORTS"
	echo "GIT_URL=${GH_PORTS}" >> ${_pdconf}
	echo "USE_TMPFS=data" >> ${_pdconf}
	echo "BASEFS=$POUDRIERE_BASEFS" >> ${_pdconf}
	echo "ATOMIC_PACKAGE_REPOSITORY=no" >> ${_pdconf}
	echo "PKG_REPO_FROM_HOST=yes" >> ${_pdconf}

	if [ "$(jq -r '."poudriere-conf" | length' ${TRUEOS_MANIFEST})" != "0" ] ; then
		jq -r '."poudriere-conf" | join("\n")' ${TRUEOS_MANIFEST} >> ${_pdconf}
	fi

	# If there is a custom poudriere.conf.release file in /etc we will also
	# include it. This can be used to set different tmpfs or JOBS on a per system
	# basis
	if [ -e "/etc/poudriere.conf.release" ] ; then
		cat /etc/poudriere.conf.release >> ${_pdconf}
	fi
}

setup_poudriere_jail()
{
	# Create new jail
	poudriere jail -c -j $POUDRIERE_BASE -m url=file://${DIST_DIR} -v ${OSRELEASE}
	if [ $? -ne 0 ] ; then
		exit_err "Failed creating poudriere jail"
	fi

	# Create the new ports tree
	poudriere ports -c -p $POUDRIERE_PORTS -m git -B $GH_PORTS_BRANCH
	if [ $? -ne 0 ] ; then
		exit_err "Failed creating poudriere ports"
	fi

	# Save the list of build flags
	jq -r '."ports-conf" | join("\n")' ${TRUEOS_MANIFEST} >/etc/poudriere.d/${POUDRIERE_BASE}-make.conf
}

build_poudriere()
{
	# Check if we want to do a bulk build of everything
	if [ $(jq -r '."package-all"' ${TRUEOS_MANIFEST}) = "true" ] ; then
		# Start the build
		poudriere bulk -a -j $POUDRIERE_BASE -p ${POUDRIERE_PORTS}
		check_essential_pkgs
	fi

	# Check if we want to do a selective build
	# (And yes, sometimes you want to do this after a "full" build to catch things
	# which may purposefully not be tied into the complete build process
	if [ "$(jq -r '."packages" | length' ${TRUEOS_MANIFEST})" != "0" ] ; then
		jq -r '."packages" | join("\n")' ${TRUEOS_MANIFEST} > ${OBJDIR}/trueos-mk-bulk-list

		# Start the build
		poudriere bulk -f ${OBJDIR}/trueos-mk-bulk-list -j $POUDRIERE_BASE -p ${POUDRIERE_PORTS}
		if [ $? -ne 0 ] ; then
			exit_err "Failed poudriere build"
		fi
	fi
}

clean_poudriere()
{
	# Kill previous jail
	poudriere jail -k -j $POUDRIERE_BASE -p ${POUDRIERE_PORTS}

	# Delete previous jail
	echo "poudriere jail -d -j ${POUDRIERE_BASE}"
	echo -e "y\n" | poudriere jail -d -j ${POUDRIERE_BASE}

	# Delete previous ports tree
	echo -e "y\n" | poudriere ports -d -p ${POUDRIERE_PORTS}
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

cp_iso_pkgs()
{
	mkdir -p ${OBJDIR}/repo-config
	cat >${OBJDIR}/repo-config/repo.conf <<EOF
base: {
  url: "file://${PKG_DIR}/${ABI_DIR}/latest",
  signature_type: "none",
  enabled: yes
}
ports: {
  url: "file://${POUDRIERE_PKGDIR}",
  signature_type: "none",
  enabled: yes
}

EOF
	PKG_VERSION=$(readlink ${PKG_DIR}/${ABI_DIR}/latest)
	mkdir -p ${TARGET_DIR}/${ABI_DIR}/${PKG_VERSION}
	ln -s ${PKG_VERSION} ${TARGET_DIR}/${ABI_DIR}/latest

	# Copy over the base system packages
	pkg-static -o ABI_FILE=${OBJDIR}/disc1/bin/sh \
		-R ${OBJDIR}/repo-config \
		fetch -y -o ${TARGET_DIR}/${ABI_DIR}/${PKG_VERSION} -g FreeBSD-*
	if [ $? -ne 0 ] ; then
		exit_err "Failed copying base packages to ISO..."
	fi

	# Check if we have dist-packages to include on the ISO
	if [ "$(jq -r '."dist-packages" | length' ${TRUEOS_MANIFEST})" != "0" ] ; then
		for i in $(jq -r '."dist-packages" | join(" ")' ${TRUEOS_MANIFEST})
		do
			pkg-static -o ABI_FILE=${OBJDIR}/disc1/bin/sh \
				-R ${OBJDIR}/repo-config \
				fetch -y -d -o ${TARGET_DIR}/${ABI_DIR}/${PKG_VERSION} $i
				if [ $? -ne 0 ] ; then
					exit_err "Failed copying dist-package $i to ISO..."
				fi
		done
	fi

	# Create the repo DB
	echo "Creating installer pkg repo"
	pkg repo ${TARGET_DIR}/${ABI_DIR}/${PKG_VERSION}

	if [ "$(jq -r '."auto-install-packages" | length' ${TRUEOS_MANIFEST})" != "0" ] ; then
		echo "Saving package list to auto-install"
		jq -r '."auto-install-packages" | join(" ")' ${TRUEOS_MANIFEST} \
			 >${OBJDIR}/disc1/root/auto-dist-install
	fi

	# Create the local repo DB config
	mkdir -p ${OBJDIR}/disc1/etc/pkg
	cat >${OBJDIR}/disc1/etc/pkg/TrueOS.conf <<EOF
install-repo: {
  url: "file:///dist/${ABI_DIR}/latest",
  signature_type: "none",
  enabled: yes
}
EOF

	# If there are any packages to install into the ISO, do it now
	if [ "$(jq -r '."iso-install-packages" | length' ${TRUEOS_MANIFEST})" != "0" ] ; then
		for i in $(jq -r '."iso-install-packages" | join(" ")' ${TRUEOS_MANIFEST})
		do
			pkg-static -o ABI_FILE=/bin/sh \
				-R /etc/pkg \
				-c ${OBJDIR}/disc1 \
				install -y $i
				if [ $? -ne 0 ] ; then
					exit_err "Failed installing package $i to ISO..."
				fi
		done
	fi
}

apply_iso_config()
{

	# Check for a custom install script
	_jsins=$(jq -r '."install-script"' ${TRUEOS_MANIFEST})
	if [ "$_jsins" != "null" -a -n "$_jsins" ] ; then
		echo "Setting custom install script"
		jq -r '."install-script"' ${TRUEOS_MANIFEST} \
			 >${OBJDIR}/disc1/etc/trueos-custom-install
	fi

	# Check for auto-install script
	_jsauto=$(jq -r '."auto-install-script"' ${TRUEOS_MANIFEST})
	if [ "$_jsauto" != "null" -a -n "$_jsauto" ] ; then
		echo "Setting auto install script"
		cp $(jq -r '."auto-install-script"' ${TRUEOS_MANIFEST}) \
			 ${OBJDIR}/disc1/etc/installerconfig
	fi

}

save_pkg_config()
{
	if [ "$(jq -r '."pkg-repo"."url" | length' ${TRUEOS_MANIFEST})" = "0" ]; then
		return 0
	fi

	_jspkgurl="$(jq -r '."pkg-repo"."url"' ${TRUEOS_MANIFEST})"
	echo "Saving pkg repository URL"
	echo "$_jspkgurl" > ${OBJDIR}/disc1/dist/trueos-pkg-url

	# Check if a pubkey is specified
	if [ "$(jq -r '."pkg-repo"."pubKey" | length' ${TRUEOS_MANIFEST})" != "0" ]; then
		echo "Saving pkg repository public key"
		jq -r '."pkg-repo"."pubKey" | join("\n")' ${TRUEOS_MANIFEST} \
			> ${OBJDIR}/disc1/dist/trueos-pkg-url.pubkey
	fi
}

save_base_pkg_config()
{
	if [ "$(jq -r '."base-pkg-repo"."url" | length' ${TRUEOS_MANIFEST})" = "0" ]; then
		return 0
	fi

	_jspkgurl="$(jq -r '."base-pkg-repo"."url"' ${TRUEOS_MANIFEST})"
	echo "Saving base pkg repository URL"
	echo "$_jspkgurl" > ${OBJDIR}/disc1/dist/trueos-base-pkg-url

	# Check if a pubkey is specified
	if [ "$(jq -r '."base-pkg-repo"."pubKey" | length' ${TRUEOS_MANIFEST})" != "0" ]; then
		echo "Saving pkg repository public key"
		jq -r '."base-pkg-repo"."pubKey" | join("\n")' ${TRUEOS_MANIFEST} \
			> ${OBJDIR}/disc1/dist/trueos-base-pkg-url.pubkey
	fi
}

env_check

case $1 in
	clean) clean_jails ; exit 0 ;;
	poudriere) run_poudriere ;;
	iso) cp_iso_pkgs
	     apply_iso_config
	     save_pkg_config
	     save_base_pkg_config
	     ;;
	*) echo "Unknown option selected" ;;
esac

exit 0

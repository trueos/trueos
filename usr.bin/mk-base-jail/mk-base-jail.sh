#!/bin/sh

# $FreeBSD$

# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
#  Copyright (c) 2018 Kris Moore
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
#  THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
#  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
#  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
#  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
#  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
#  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
#  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
#  SUCH DAMAGE.

usage()
{
	echo "Usage: $0 <directory>"
	echo "----------------------------"
	echo "Optional flags:"
	echo "	-t (poudriere|jail)	- Setup traditional jail or boot-strap for poudriere"
	echo "	-c (llvm60|llvm70)	- Which default compiler to load"
	exit 1
}

default_params() {
	DEFAULTCC="llvm60"
	JTYPE="jail"
}

# Parse the command line
parse_cmdline() {
	while [ $# -gt 0 ]; do
		case "$1" in
		-t)
			if [ $# -eq 1 ]; then usage; fi
			if [ ! -z "${GITBRANCH}" ]; then usage; fi
			shift; JTYPE="$1"
			case ${JTYPE} in
				poudriere|jail) ;;
				*) usage ;;
			esac
			;;
		-c)
			if [ $# -eq 1 ]; then usage; fi
			if [ ! -z "${CONFFILE}" ]; then usage; fi
			shift; DEFAULTCC="$1"
			;;
		-h | --help | help)
			usage
			;;
		*)
			if [ $# -gt 1 ]; then usage; fi
			TDIR="$1"
			;;
		esac
		shift
	done

	if [ ! -d "$TDIR" ] ; then
		echo "Target directory: $1 does not exist!"
		exit 1
	fi

	if [ "$TDIR" = "/" ]; then
		echo "ERROR: Target dir is: '/' ???"
		exit 1
	fi
}

pkg_install_jail()
{
	# Get basename and install runtime first
	pkg -r "${1}" update
	BASENAME=$(pkg-static -r "$1" rquery -U '%o %n-%v' | grep '^base ' | grep -e '-runtime-' | head -n 1 | awk '{print $2}' | cut -d '-' -f 1)

	# Fetch all the packages first
	pkg-static -r "${1}" fetch -U -d -y -g ${BASENAME}-*
	if [ $? -ne 0 ] ; then
		echo "Failed fetching base packages"
		exit 1
	fi

	pkg-static -r "${1}" install -U -y ${BASENAME}-runtime
	if [ $? -ne 0 ] ; then
		echo "Failed installing ${BASENAME}-runtime"
		exit 1
	fi

	# Install everything except debug
	PLIST=$(pkg-static -r "$1" rquery -U '%o %n' | grep '^base ' | awk '{print $2}' | grep -v -- '-debug' | grep -v -- '-kernel' | tr -s '\n' ' ')
	for pkg in $PLIST
	do
		pkg-static -r "${1}" install -U -y ${pkg}
		if [ $? -ne 0 ] ; then
			echo "Failed installing ${pkg}"
			exit 1
		fi
	done

	# Verify we included a default compiler
	pkg -r "$1" info -q ${DEFAULTCC}
	if [ $? -ne 0 ] ; then
		pkg-static -r "${1}" install -U -y ${DEFAULTCC}
		if [ $? -ne 0 ] ; then
			echo "Failed installing: ${DEFAULTCC}"
			exit 1
		fi
	fi
}

boot_strap_cc()
{
	chroot ${1} compiler-bootstrap ${DEFAULTCC}
}

prep_poudriere()
{
	if [ "$1" = "/" ] ; then
		echo "FATAL: TDIR is /"
		exit 1
	fi

	# For poudriere we have some additional steps to do to make it safe for building ports
	# This is to boot-strap our compiler and then un-register any packages, so it appears
	# More like a dist or source compiled world

	# Start by grabbing system sources
	DURL=$(cat /etc/pkg/TrueOS.conf | grep url: | awk '{print $2}' | cut -d '"' -f 2 | sed 's|/pkg/|/iso/|g' | sed 's|${ABI}/latest|dist|g')
	fetch -o "${1}/src.txz" ${DURL}/src.txz
	if [ $? -ne 0 ] ; then
		echo "WARNING: Failed fetching system sources!"
		echo "Please fetch and extract to ${1}/usr/src"
	else
		mkdir -p "${1}/usr/src"
		tar xpf "${1}/src.txz" -C "${1}"
		if [ $? -ne 0 ] ; then
			echo "WARNING: Failed extracting src.txz"
		fi
		rm "${1}/src.txz"
	fi

	if [ -d "${1}/usr/local/llvm60" ] ; then
		CDIR="${1}/usr/local/llvm60"
	fi
	if [ -d "${1}/usr/local/llvm70" ] ; then
		CDIR="${1}/usr/local/llvm70"
	fi
	if [ -z "$CDIR" ] ; then
		echo "WARNING: unknown default compiler!"
		return 0
	fi

	# Copy over a few shared libs needed for llvm
	cp -a ${1}/usr/local/lib/libedit* "${CDIR}/lib/"
	if [ $? -ne 0 ] ; then
		echo "Failed copying libedit*"
		exit 1
	fi
	cp -a ${1}/usr/local/lib/libxml* "${CDIR}/lib/"
	if [ $? -ne 0 ] ; then
		echo "Failed copying libxml*"
		exit 1
	fi

	mv "${CDIR}" "${1}/tmpcc"
	if [ $? -ne 0 ] ; then
		echo "Failed moving $CDIR"
		exit 1
	fi

	rm -rf "${1}/usr/local"
	mkdir -p "${1}/usr/local"

	mv "${1}/tmpcc" "${CDIR}"
	if [ $? -ne 0 ] ; then
		echo "Failed moving $CDIR"
		exit 1
	fi

	# Remove pkg registration
	rm -rf "${1}/var/db/pkg"

}

default_params

parse_cmdline $@

echo "Preparing to install $JTYPE base to $TDIR"

pkg_install_jail "$TDIR"

if [ "$JTYPE" = "poudriere" ] ; then
	prep_poudriere "$TDIR"
fi

boot_strap_cc "$TDIR"

echo "Finished preparing $JTYPE base at $TDIR"
if [ "$JTYPE" = "poudriere" ] ; then
	echo "To import this poudriere jail, run:"
	echo "poudriere jail -c -j myjailname -m null -M ${TDIR} -v 12.0-RELEASE"
fi


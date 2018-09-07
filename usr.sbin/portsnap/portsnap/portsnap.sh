#!/bin/sh

#-
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright 2004-2005 Colin Percival
# Copyright 2018 Kris Moore
# All rights reserved
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted providing that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
# IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

# $FreeBSD$

usage() {
	cat <<EOF
usage: `basename $0` [options] command ... [path]

Options:
  -b branch    -- Git branch to use from remote
                  (default: trueos-master)
  -f conffile  -- Read configuration options from conffile
                  (default: /etc/portsnap.conf)
  -p portsdir  -- Location of uncompressed ports tree
                  (default: /usr/ports/)
  -r remote    -- Git Server from which to fetch updates.
                  (default: https://github.com/trueos/trueos-ports)
Commands:
  cron         -- Sleep rand(3600) seconds, and then extract updates.
  extract      -- Extract snapshot of ports tree, replacing existing
                  files and directories.
  update       -- Update ports tree to match current snapshot, replacing
                  files and directories which have changed.
  auto         -- Fetch updates, and either extract a new ports tree or
                  update an existing tree.
  fetch        -- Fetch updates - Dummy shim for compat reasons
EOF
	exit 0
}

#### Parameter handling functions.

# Initialize parameters to null, just in case they're
# set in the environment.
init_params() {
	PORTSDIR=""
	CONFFILE=""
	COMMAND=""
	COMMANDS=""
	GITREMOTE="https://github.com/trueos/trueos-ports"
	GITBRANCH="trueos-master"
}

# Parse the command line
parse_cmdline() {
	while [ $# -gt 0 ]; do
		case "$1" in
		-b)
			if [ $# -eq 1 ]; then usage; fi
			if [ ! -z "${GITBRANCH}" ]; then usage; fi
			shift; GITBRANCH="$1"
			;;
		-f)
			if [ $# -eq 1 ]; then usage; fi
			if [ ! -z "${CONFFILE}" ]; then usage; fi
			shift; CONFFILE="$1"
			;;
		-h | --help | help)
			usage
			;;
		-d)
			if [ $# -eq 1 ]; then usage; fi
			shift; DATADIR="$1"
			;;
		-p)
			if [ $# -eq 1 ]; then usage; fi
			if [ ! -z "${PORTSDIR}" ]; then usage; fi
			shift; PORTSDIR="$1"
			;;
		-r)
			if [ $# -eq 1 ]; then usage; fi
			if [ ! -z "${GITREMOTE}" ]; then usage; fi
			shift; GITREMOTE="$1"
			;;
		cron | extract | fetch | update | auto)
			COMMANDS="${COMMANDS} $1"
			;;
		up)
			COMMANDS="${COMMANDS} update"
			;;
		alfred)
			COMMANDS="${COMMANDS} auto"
			;;
		*)
			if [ $# -gt 1 ]; then usage; fi
			if echo ${COMMANDS} | grep -vq extract; then
				usage
			fi
			EXTRACTPATH="$1"
			;;
		esac
		shift
	done

	if [ -z "${COMMANDS}" ]; then
		usage
	fi
}

# If CONFFILE was specified at the command-line, make
# sure that it exists and is readable.
sanity_conffile() {
	if [ ! -z "${CONFFILE}" ] && [ ! -r "${CONFFILE}" ]; then
		echo -n "File does not exist "
		echo -n "or is not readable: "
		echo ${CONFFILE}
		exit 1
	fi
}

# If a configuration file hasn't been specified, use
# the default value (/etc/portsnap.conf)
default_conffile() {
	if [ -z "${CONFFILE}" ]; then
		CONFFILE="/etc/portsnap.conf"
	fi
}

# Read {GITREMOTE, PORTSDIR, GITBRANCH} from the configuration
# file if they haven't already been set.  If the configuration
# file doesn't exist, do nothing.
parse_conffile() {
	if [ -r "${CONFFILE}" ]; then
		for X in PORTSDIR GITREMOTE GITBRANCH; do
			eval _=\$${X}
			if [ -z "${_}" ]; then
				eval ${X}=`grep "^${X}=" "${CONFFILE}" |
				    cut -f 2- -d '=' | tail -1`
			fi
		done
	fi
}

# If parameters have not been set, use default values
default_params() {
	_PORTSDIR="/usr/ports"
	_GITREMOTE="https://github.com/trueos/trueos-ports"
	_GITBRANCH="trueos-master"
	for X in PORTSDIR GITREMOTE GITBRANCH; do
		eval _=\$${X}
		eval __=\$_${X}
		if [ -z "${_}" ]; then
			eval ${X}=${__}
		fi
	done
}

# Perform sanity checks and set some final parameters
# in preparation for extracting or updating ${PORTSDIR}
# Complain if ${PORTSDIR} exists but is not writable,
# but don't complain if ${PORTSDIR} doesn't exist.
extract_check_params() {
	_PORTSDIR_bad="Directory is not writable: "

	if [ -d "${PORTSDIR}" ] && ! [ -w "${PORTSDIR}" ]; then
		echo -n "`basename $0`: "
		echo -n "${_PORTSDIR_bad}"
		echo ${PORTSDIR}
		exit 1
	fi
}

# Perform sanity checks and set some final parameters
# in preparation for updating ${PORTSDIR}
update_check_params() {
	extract_check_params

	if ! [ -r ${PORTSDIR}/.git ]; then
		echo "${PORTSDIR} was not created by git."
		echo -n "You must run '`basename $0` extract' before "
		echo "running '`basename $0` update'."
		exit 1
	fi

}

#### Core functionality -- the actual work gets done here

# Do the actual work involved in "extract"
extract_run() {
	mkdir -p ${PORTSDIR} || return 1

	if [ -d ${PORTSDIR}/.git ] ; then
		rm -rf ${PORTSDIR}/.git
	fi

	cd ${PORTSDIR} || return 1
	git init || return 1
	git remote add origin $GITREMOTE || return 1
	git fetch --depth=1 || return 1
	git reset --hard origin/$GITBRANCH || return 1
	git checkout $GITBRANCH || return 1
	git clean -f -d || return 1
}

update_run_extract() {
	cd ${PORTSDIR} || return 1
	git fetch || return 1
	git reset --hard origin/$GITBRANCH || return 1
	git checkout $GITBRANCH || return 1
}

# Do the actual work involved in "update"
update_run() {
	update_run_extract || return 1
}

#### Main functions -- call parameter-handling and core functions

# Using the command line, configuration file, and defaults,
# set all the parameters which are needed later.
get_params() {
	init_params
	parse_cmdline $@
	sanity_conffile
	default_conffile
	parse_conffile
	default_params
}

# Cron command.  Make sure the parameters are sensible; wait
# rand(3600) seconds; then fetch updates.  While fetching updates,
# send output to a temporary file; only print that file if the
# fetching failed.
cmd_cron() {
	sleep `jot -r 1 0 3600`
}

# Extract command.  Make sure the parameters are sensible,
# then extract the ports tree (or part thereof).
cmd_extract() {
	extract_check_params
	extract_run || exit 1
}

# Update command.  Make sure the parameters are sensible,
# then update the ports tree.
cmd_update() {
	update_check_params
	update_run || exit 1
}

# Auto command. Run 'update' or
# 'extract' depending on whether ${PORTSDIR} exists.
cmd_auto() {
	if [ -r ${PORTSDIR}/.git ]; then
		cmd_update
	else
		cmd_extract
	fi
}

cmd_fetch() {
	return 0
}

#### Entry point

# Make sure we find utilities from the base system
export PATH=/sbin:/bin:/usr/sbin:/usr/bin:${PATH}

# Set LC_ALL in order to avoid problems with character ranges like [A-Z].
export LC_ALL=C

which -s git
if [ $? -ne 0 ] ; then
	echo "The 'git' command was not found."
	echo "Please install with 'pkg install git' and try again"
fi

get_params $@
for COMMAND in ${COMMANDS}; do
	cmd_${COMMAND}
done

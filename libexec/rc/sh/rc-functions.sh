# Copyright (c) 2007 Gentoo Foundation
# Copyright (c) 2007-2009 Roy Marples <roy@marples.name>
# Released under the 2-clause BSD license.

has_addon()
{
	[ -e /lib/rc/addons/"$1".sh -o -e /lib/rcscripts/addons/"$1".sh ]
}

_addon_warn()
{
	eindent
	ewarn "$RC_SVCNAME uses addon code which is deprecated"
	ewarn "and may not be available in the future."
	eoutdent
}

import_addon()
{
	if [ -e /lib/rc/addons/"$1".sh ]; then
		_addon_warn
		. /lib/rc/addons/"$1".sh
	elif [ -e /lib/rcscripts/addons/"$1".sh ]; then
		_addon_warn
		. /lib/rcscripts/addons/"$1".sh
	else
		return 1
	fi
}

start_addon()
{
	( import_addon "$1-start" )
}

stop_addon()
{
	( import_addon "$1-stop" )
}

net_fs_list="afs ceph cifs coda davfs fuse fuse.sshfs gfs glusterfs lustre
ncpfs nfs nfs4 ocfs2 shfs smbfs"
is_net_fs()
{
	[ -z "$1" ] && return 1

	# Check OS specific flags to see if we're local or net mounted
	mountinfo --quiet --netdev "$1"  && return 0
	mountinfo --quiet --nonetdev "$1" && return 1

	# Fall back on fs types
	local t=$(mountinfo --fstype "$1")
	for x in $net_fs_list $extra_net_fs_list; do
		[ "$x" = "$t" ] && return 0
	done
	return 1
}

is_union_fs()
{
	[ ! -x /sbin/unionctl ] && return 1
	unionctl "$1" --list >/dev/null 2>&1
}

get_bootparam()
{
	local match="$1"
	[ -z "$match" -o ! -r /proc/cmdline ] && return 1

	set -- $(cat /proc/cmdline)
	while [ -n "$1" ]; do
		[ "$1" = "$match" ] && return 0
		case "$1" in
			gentoo=*)
				local params="${1##*=}"
				local IFS=, x=
				for x in $params; do
					[ "$x" = "$match" ] && return 0
				done
				;;
		esac
		shift
	done

	return 1
}

# Called from openrc-run.sh or gendepends.sh
_get_containers() {
	local c
	case "${RC_UNAME}" in
	FreeBSD)
		c="-jail"
		;;
	Linux)
		c="-docker -lxc -openvz -rkt -systemd-nspawn -uml -vserver"
		;;
	esac
	echo $c
}

_get_containers_remove() {
	local c
	for x in $(_get_containers); do
		c="${c}!${x} "
	done
	echo $c
}

_depend() {
	depend
	local _rc_svcname=$(shell_var "$RC_SVCNAME") _deptype= _depends=

	# Add any user defined depends
	for _deptype in config:CONFIG need:NEED use:USE want:WANT \
	after:AFTER before:BEFORE \
	provide:PROVIDE keyword:KEYWORD; do
		IFS=:
		set -- $_deptype
		unset IFS
		eval _depends=\$rc_${_rc_svcname}_$1
		[ -z "$_depends" ] && eval _depends=\$rc_$1
		[ -z "$_depends" ] && eval _depends=\$RC_${_rc_svcname}_$2
		[ -z "$_depends" ] && eval _depends=\$RC_$2

		$1 $_depends
	done
}

# Code common to scripts that need to load a kernel module
# if it isn't in the kernel yet. Syntax:
#   load_kld [-e regex] [-m module] file
# where -e or -m chooses the way to check if the module
# is already loaded:
#   regex is egrep'd in the output from `kldstat -v',
#   module is passed to `kldstat -m'.
# The default way is as though `-m file' were specified.
load_kld()
{
	local _loaded _mod _opt _re

	while getopts "e:m:" _opt; do
		case "$_opt" in
		e) _re="$OPTARG" ;;
		m) _mod="$OPTARG" ;;
		*) eend 3 'USAGE: load_kld [-e regex] [-m module] file' ;;
		esac
	done
	shift $(($OPTIND - 1))
	if [ $# -ne 1 ]; then
		eend 3 'USAGE: load_kld [-e regex] [-m module] file'
	fi
	_mod=${_mod:-$1}
	_loaded=false
	if [ -n "$_re" ]; then
		if kldstat -v | egrep -q -e "$_re"; then
			_loaded=true
		fi
	else
		if kldstat -q -m "$_mod"; then
			_loaded=true
		fi
	fi
	if ! $_loaded; then
		if ! kldload "$1"; then
			ewarn "Unable to load kernel module $1"
			return 1
		fi
	fi
	return 0
}

# Add our sbin to $PATH
case "$PATH" in
	"$RC_LIBEXECDIR"/sbin|"$RC_LIBEXECDIR"/sbin:*);;
	*) PATH="$RC_LIBEXECDIR/sbin:$PATH" ; export PATH ;;
esac

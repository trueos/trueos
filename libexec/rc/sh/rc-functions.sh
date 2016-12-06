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

# list_net_interfaces type
#	List all network interfaces. The type of interface returned
#	can be controlled by the type argument. The type
#	argument can be any of the following:
#		nodhcp	- all interfaces, excluding DHCP configured interfaces
#		dhcp	- list only DHCP configured interfaces
#		noautoconf	- all interfaces, excluding IPv6 Stateless
#				  Address Autoconf configured interfaces
#		autoconf	- list only IPv6 Stateless Address Autoconf
#				  configured interfaces
#	If no argument is specified all network interfaces are output.
#	Note that the list will include cloned interfaces if applicable.
#	Cloned interfaces must already exist to have a chance to appear
#	in the list if ${network_interfaces} is set to `auto'.
#
list_net_interfaces()
{
	local type _tmplist _list _autolist _lo _if
	type=$1

	# Get a list of ALL the interfaces and make lo0 first if it's there.
	#
	_tmplist=
	case ${network_interfaces} in
	[Aa][Uu][Tt][Oo])
		_autolist="`${IFCONFIG_CMD} -l`"
		_lo=
		for _if in ${_autolist} ; do
			if autoif $_if; then
				if [ "$_if" = "lo0" ]; then
					_lo="lo0 "
				else
					_tmplist="${_tmplist} ${_if}"
				fi
			fi
		done
		_tmplist="${_lo}${_tmplist# }"
	;;
	*)
		for _if in ${network_interfaces} ${cloned_interfaces}; do
			# epair(4) uses epair[0-9] for creation and
			# epair[0-9][ab] for configuration.
			case $_if in
			epair[0-9]*)
				_tmplist="$_tmplist ${_if}a ${_if}b"
			;;
			*)
				_tmplist="$_tmplist $_if"
			;;
			esac
		done
		#
		# lo0 is effectively mandatory, so help prevent foot-shooting
		#
		case "$_tmplist" in
		lo0|'lo0 '*|*' lo0'|*' lo0 '*)
			# This is fine, do nothing
			_tmplist="${_tmplist# }"
		;;
		*)
			_tmplist="lo0 ${_tmplist# }"
		;;
		esac
	;;
	esac

	_list=
	case "$type" in
	nodhcp)
		for _if in ${_tmplist} ; do
			if ! dhcpif $_if && \
			   [ -n "`_ifconfig_getargs $_if`" ]; then
				_list="${_list# } ${_if}"
			fi
		done
	;;
	dhcp)
		for _if in ${_tmplist} ; do
			if dhcpif $_if; then
				_list="${_list# } ${_if}"
			fi
		done
	;;
	noautoconf)
		for _if in ${_tmplist} ; do
			if ! ipv6_autoconfif $_if && \
			   [ -n "`_ifconfig_getargs $_if ipv6`" ]; then
				_list="${_list# } ${_if}"
			fi
		done
	;;
	autoconf)
		for _if in ${_tmplist} ; do
			if ipv6_autoconfif $_if; then
				_list="${_list# } ${_if}"
			fi
		done
	;;
	*)
		_list=${_tmplist}
	;;
	esac

	echo $_list

	return 0
}

# ltr str src dst [var]
#	Change every $src in $str to $dst.
#	Useful when /usr is not yet mounted and we cannot use tr(1), sed(1) nor
#	awk(1). If var is non-NULL, set it to the result.
ltr()
{
	local _str _src _dst _out _com _var
	_str="$1"
	_src="$2"
	_dst="$3"
	_var="$4"
	_out=""

	local IFS="${_src}"
	for _com in ${_str}; do
		if [ -z "${_out}" ]; then
			_out="${_com}"
		else
			_out="${_out}${_dst}${_com}"
		fi
	done
	if [ -n "${_var}" ]; then
		setvar "${_var}" "${_out}"
	else
		echo "${_out}"
	fi
}

# get_if_var if var [default]
#	Return the value of the pseudo-hash corresponding to $if where
#	$var is a string containg the sub-string "IF" which will be
#	replaced with $if after the characters defined in _punct are
#	replaced with '_'. If the variable is unset, replace it with
#	$default if given.
get_if_var()
{
	local _if _punct _punct_c _var _default prefix suffix

	if [ $# -ne 2 -a $# -ne 3 ]; then
		err 3 'USAGE: get_if_var name var [default]'
	fi

	_if=$1
	_punct=".-/+"
	ltr ${_if} "${_punct}" '_' _if
	_var=$2
	_default=$3

	prefix=${_var%%IF*}
	suffix=${_var##*IF}
	eval echo \${${prefix}${_if}${suffix}-${_default}}
}

# _ifconfig_getargs if [af]
#	Prints the arguments for the supplied interface to stdout.
#	Returns 1 if empty.  In general, ifconfig_getargs should be used
#	outside this file.
_ifconfig_getargs()
{
	local _ifn _af
	_ifn=$1
	_af=${2+_$2}

	if [ -z "$_ifn" ]; then
		return 1
	fi

	get_if_var $_ifn ifconfig_IF$_af "$ifconfig_DEFAULT"
}

# ifconfig_getargs if [af]
#	Takes the result from _ifconfig_getargs and removes pseudo
#	args such as DHCP and WPA.
ifconfig_getargs()
{
	local _tmpargs _arg _args _vnet
	_tmpargs=`_ifconfig_getargs $1 $2`
	if [ $? -eq 1 ]; then
		return 1
	fi
	_args=
	_vnet=0

	for _arg in $_tmpargs; do
		case $_arg:$_vnet in
		[Ii][Pp][Vv][6]:0) ;;
		[Aa][Uu][Tt][Oo]:0) ;;
		[Dd][Hh][Cc][Pp]:0) ;;
		[Nn][Oo][Aa][Uu][Tt][Oo]:0) ;;
		[Nn][Oo][Ss][Yy][Nn][Cc][Dd][Hh][Cc][Pp]:0) ;;
		[Ss][Yy][Nn][Cc][Dd][Hh][Cc][Pp]:0) ;;
		[Ww][Pp][Aa]:0) ;;
		[Hh][Oo][Ss][Tt][Aa][Pp]:0) ;;
		vnet:0)	_vnet=1 ;;
		*:1)	_vnet=0 ;;
		*:0)
			_args="$_args $_arg"
		;;
		esac
	done

	echo $_args
}


# check_kern_features mib
#	Return existence of kern.features.* sysctl MIB as true or
#	false.  The result will be cached in $_rc_cache_kern_features_
#	namespace.  "0" means the kern.features.X exists.

check_kern_features()
{
	local _v

	[ -n "$1" ] || return 1;
	eval _v=\$_rc_cache_kern_features_$1
	[ -n "$_v" ] && return "$_v";

	if ${SYSCTL_N} kern.features.$1 > /dev/null 2>&1; then
		eval _rc_cache_kern_features_$1=0
		return 0
	else
		eval _rc_cache_kern_features_$1=1
		return 1
	fi
}

# autoif
#	Returns 0 if the interface should be automatically configured at
#	boot time and 1 otherwise.
autoif()
{
	local _tmpargs _arg
	_tmpargs=`_ifconfig_getargs $1`

	for _arg in $_tmpargs; do
		case $_arg in
		[Nn][Oo][Aa][Uu][Tt][Oo])
			return 1
			;;
		esac
	done

	return 0
}

# dhcpif if
#	Returns 0 if the interface is a DHCP interface and 1 otherwise.
dhcpif()
{
	local _tmpargs _arg
	_tmpargs=`_ifconfig_getargs $1`

	case $1 in
	lo[0-9]*|\
	stf[0-9]*|\
	lp[0-9]*|\
	sl[0-9]*)
		return 1
		;;
	esac
	if noafif $1; then
		return 1
	fi

	for _arg in $_tmpargs; do
		case $_arg in
		[Aa][Uu][Tt][Oo])
			return 0
			;;
		[Dd][Hh][Cc][Pp])
			return 0
			;;
		[Nn][Oo][Ss][Yy][Nn][Cc][Dd][Hh][Cc][Pp])
			return 0
			;;
		[Ss][Yy][Nn][Cc][Dd][Hh][Cc][Pp])
			return 0
			;;
		esac
	done

	return 1
}

# syncdhcpif
#	Returns 0 if the interface should be configured synchronously and
#	1 otherwise.
syncdhcpif()
{
	local _tmpargs _arg
	_tmpargs=`_ifconfig_getargs $1`

	if noafif $1; then
		return 1
	fi

	for _arg in $_tmpargs; do
		case $_arg in
		[Nn][Oo][Ss][Yy][Nn][Cc][Dd][Hh][Cc][Pp])
			return 1
			;;
		[Ss][Yy][Nn][Cc][Dd][Hh][Cc][Pp])
			return 0
			;;
		esac
	done

	yesno synchronous_dhclient
}

# wpaif if
#	Returns 0 if the interface is a WPA interface and 1 otherwise.
wpaif()
{
	local _tmpargs _arg
	_tmpargs=`_ifconfig_getargs $1`

	for _arg in $_tmpargs; do
		case $_arg in
		[Ww][Pp][Aa])
			return 0
			;;
		esac
	done

	return 1
}

# hostapif if
#	Returns 0 if the interface is a HOSTAP interface and 1 otherwise.
hostapif()
{
	local _tmpargs _arg
	_tmpargs=`_ifconfig_getargs $1`

	for _arg in $_tmpargs; do
		case $_arg in
		[Hh][Oo][Ss][Tt][Aa][Pp])
			return 0
			;;
		esac
	done

	return 1
}

# vnetif if
#	Returns 0 and echo jail if "vnet" keyword is specified on the
#	interface, and 1 otherwise.
vnetif()
{
	local _tmpargs _arg _vnet
	_tmpargs=`_ifconfig_getargs $1`

	_vnet=0
	for _arg in $_tmpargs; do
		case $_arg:$_vnet in
		vnet:0)	_vnet=1 ;;
		*:1)	echo $_arg; return 0 ;;
		esac
	done

	return 1
}

# afexists af
#	Returns 0 if the address family is enabled in the kernel
#	1 otherwise.
afexists()
{
	local _af
	_af=$1

	case ${_af} in
	inet|inet6)
		check_kern_features ${_af}
		;;
	atm)
		if [ -x /sbin/atmconfig ]; then
			/sbin/atmconfig diag list > /dev/null 2>&1
		else
			return 1
		fi
		;;
	link|ether)
		return 0
		;;
	*)
		err 1 "afexists(): Unsupported address family: $_af"
		;;
	esac
}

# noafif if
#	Returns 0 if the interface has no af configuration and 1 otherwise.
noafif()
{
	local _if
	_if=$1

	case $_if in
	pflog[0-9]*|\
	pfsync[0-9]*|\
	usbus[0-9]*|\
	an[0-9]*|\
	ath[0-9]*|\
	ipw[0-9]*|\
	ipfw[0-9]*|\
	iwi[0-9]*|\
	iwn[0-9]*|\
	ral[0-9]*|\
	wi[0-9]*|\
	wl[0-9]*|\
	wpi[0-9]*)
		return 0
		;;
	esac

	return 1
}

# ipv6if if
#	Returns 0 if the interface should be configured for IPv6 and
#	1 otherwise.
ipv6if()
{
	local _if _tmpargs i
	_if=$1

	if ! afexists inet6; then
		return 1
	fi

	# lo0 is always IPv6-enabled
	case $_if in
	lo0)
		return 0
		;;
	esac

	case "${ipv6_network_interfaces}" in
	$_if|"$_if "*|*" $_if"|*" $_if "*|[Aa][Uu][Tt][Oo])
		# True if $ifconfig_IF_ipv6 is defined.
		_tmpargs=`_ifconfig_getargs $_if ipv6`
		if [ -n "${_tmpargs}" ]; then
			return 0
		fi

		# True if $ipv6_prefix_IF is defined.
		_tmpargs=`get_if_var $_if ipv6_prefix_IF`
		if [ -n "${_tmpargs}" ]; then
			return 0
		fi

		# backward compatibility: True if $ipv6_ifconfig_IF is defined.
		_tmpargs=`get_if_var $_if ipv6_ifconfig_IF`
		if [ -n "${_tmpargs}" ]; then
			return 0
		fi
		;;
	esac

	return 1
}

# ipv6_autoconfif if
#	Returns 0 if the interface should be configured for IPv6 with
#	Stateless Address Configuration; 1 otherwise.
ipv6_autoconfif()
{
	local _if _tmpargs _arg
	_if=$1

	case $_if in
	lo[0-9]*|\
	stf[0-9]*|\
	lp[0-9]*|\
	sl[0-9]*)
		return 1
		;;
	esac
	if noafif $_if; then
		return 1
	fi
	if ! ipv6if $_if; then
		return 1
	fi
	if yesno ipv6_gateway_enable; then
		return 1
	fi
	_tmpargs=`get_if_var $_if ipv6_prefix_IF`
	if [ -n "${_tmpargs}" ]; then
		return 1
	fi
	# backward compatibility: $ipv6_enable
	case $ipv6_enable in
	[Yy][Ee][Ss]|[Tt][Rr][Uu][Ee]|[Oo][Nn]|1)
		if yesno ipv6_gateway_enable; then
			return 1
		fi
		case $1 in
		bridge[0-9]*)
			# No accept_rtadv by default on if_bridge(4)
			# to avoid a conflict with the member
			# interfaces.
			return 1
		;;
		*)
			return 0
		;;
		esac
	;;
	esac

	_tmpargs=`_ifconfig_getargs $_if ipv6`
	for _arg in $_tmpargs; do
		case $_arg in
		accept_rtadv)
			return 0
			;;
		esac
	done

	# backward compatibility: $ipv6_ifconfig_IF
	_tmpargs=`get_if_var $_if ipv6_ifconfig_IF`
	for _arg in $_tmpargs; do
		case $_arg in
		accept_rtadv)
			return 0
			;;
		esac
	done

	return 1
}

# ifexists if
#	Returns 0 if the interface exists and 1 otherwise.
ifexists()
{
	[ -z "$1" ] && return 1
	${IFCONFIG_CMD} -n $1 > /dev/null 2>&1
}

# Location to ifconfig
export IFCONFIG_CMD="/sbin/ifconfig"
export SYSCTL="/sbin/sysctl"
export SYSCTL_N="${SYSCTL} -n"

# Add our sbin to $PATH
case "$PATH" in
	"$RC_LIBEXECDIR"/sbin|"$RC_LIBEXECDIR"/sbin:*);;
	*) PATH="$RC_LIBEXECDIR/sbin:$PATH" ; export PATH ;;
esac

#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
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
#
# $FreeBSD$
#####################################################################

TMPFILE=${HOME}/.HWjson

add_to_json_str()
{
	val=$(echo $2 | sed 's|"||g'| xargs echo -n)
	jq ".${1} = \"$val\"" ${TMPFILE} > ${TMPFILE}.new
	mv ${TMPFILE}.new ${TMPFILE}
}

add_to_json_array()
{
	val=$(echo $2 | sed 's|"||g'| xargs echo -n)
	jq ".${1} += [\"$val\"]" ${TMPFILE} > ${TMPFILE}.new
	mv ${TMPFILE}.new ${TMPFILE}
}

add_to_json_obj()
{
	val=$(echo $3 | sed 's|"||g'| xargs echo -n)
	jq ".${1} += {\"$2\":\"$val\"}" ${TMPFILE} > ${TMPFILE}.new
	mv ${TMPFILE}.new ${TMPFILE}
}

DMESG="/var/run/dmesg.boot"

if [ ! -e $DMESG ] ; then
   echo "Missing $DMESG"
   exit 1
fi

echo "{ }" > ${TMPFILE}

################################################
# Motherboard information
################################################

SERIAL=$(/bin/kenv -q smbios.system.serial)
add_to_json_str "hardware.motherboard.serial" "$SERIAL"
UUID=$(/bin/kenv -q smbios.system.uuid)
add_to_json_str "hardware.motherboard.UUID" "$UUID"
BIOSVER=$(/bin/kenv -q smbios.bios.version)
add_to_json_str "hardware.motherboard.bios" "$BIOSVER"
PMAKER=$(/bin/kenv -q smbios.planar.maker)
add_to_json_str "hardware.motherboard.maker" "$PMAKER"
PSERIAL=$(/bin/kenv -q smbios.planar.serial)
add_to_json_str "hardware.motherboard.serial" "$PSERIAL"
PPRODUCT=$(/bin/kenv -q smbios.planar.product)
add_to_json_str "hardware.motherboard.product" "$PPRODUCT"
PVER=$(/bin/kenv -q smbios.planar.version)
add_to_json_str "hardware.motherboard.version" "$PVER"


################################################
# CPU information
################################################

CPU=$(sysctl -n hw.model)
add_to_json_str "hardware.cpu.name" "$CPU"
CPUS=$(sysctl -n kern.smp.cpus)
add_to_json_str "hardware.cpu.cores" "$CPUS"
CPUSTEP=$(cat $DMESG | grep " Stepping=" | awk '{print $5}' | cut -d '=' -f 2)
add_to_json_str "hardware.cpu.stepping" "$CPUSTEP"
CPUMODEL=$(cat $DMESG | grep " Stepping=" | awk '{print $4}' | cut -d '=' -f 2)
add_to_json_str "hardware.cpu.model" "$CPUMODEL"
CPUFAM=$(cat $DMESG | grep " Stepping=" | awk '{print $3}' | cut -d '=' -f 2)
add_to_json_str "hardware.cpu.family" "$CPUFAM"
CPUID=$(cat $DMESG | grep " Stepping=" | awk '{print $2}' | cut -d '=' -f 2)
add_to_json_str "hardware.cpu.id" "$CPUID"
CPUORIGIN=$(cat $DMESG | grep " Stepping=" | awk '{print $1}' | cut -d '=' -f 2)
add_to_json_str "hardware.cpu.origin" "$CPUORIGIN"

################################################
# Memory information
################################################

PHYSMEM=`sysctl -n hw.physmem`
add_to_json_obj "hardware.memory.physical" "raw" "$PHYSMEM"
PHYSMEMMB=$(( $PHYSMEM/1048576 ))
add_to_json_obj "hardware.memory.physical" "mb" "$PHYSMEMMB"
PHYSMEM_PAGES=$(( $PHYSMEM/4 ))
add_to_json_obj "hardware.memory.physical" "pages" "$PHYSMEM_PAGES"
USERMEM=`sysctl -n hw.usermem`
add_to_json_obj "hardware.memory.user" "raw" "$USERMEM"
REAL_MEM_STR=$(grep 'real memory' $DMESG)
add_to_json_obj "hardware.memory.real" "raw_string" "$REAL_MEM_STR"
REAL_MEM_KB=$(echo ${REAL_MEM_STR} | awk '{print $4}')
add_to_json_obj "hardware.memory.real" "kb" "$REAL_MEM_KB"
REAL_MEM_MB=$(echo ${REAL_MEM_STR} | sed -e 's/(//g' | awk '{print $5}')
add_to_json_obj "hardware.memory.real" "mb" "$REAL_MEM_MB"

################################################
# Network information
################################################

NICS=$(/sbin/ifconfig -l ether)
for n in $NICS; do
	MAC=$(ifconfig ${n} | grep ether | awk '{print $2}')
	if [ -n "${MAC}" ]; then
		add_to_json_obj "hardware.network.${n}" "mac" "$MAC"
	fi
	MEDIA=$(ifconfig ${n} | grep media: | cut -d ':' -f 2 | tr -s ' ')
	if [ -n "${MEDIA}" ]; then
		add_to_json_obj "hardware.network.${n}" "media" "$MEDIA"
	fi
done

################################################
# IPMI information
################################################
if [ -e "/usr/local/bin/ipmitool" ] ; then
	IPMIMAC=$(/usr/local/bin/ipmitool lan print | grep 'MAC Address' | awk '{print $4}')
	IPMIMAC=$(echo $IPMIMAC | sed -e 's/\;//' | sed -e 's/\:/%3A/g' | sed -e 's/\;/%3B/g')
	add_to_json_str "hardware.ipmi.mac" "$IPMIMAC"
	IPMIIP=$(/usr/local/bin/ipmitool lan print | grep 'IP Address' | awk '{print $4}' | grep -v :)
fi

################################################
# Storage information
################################################

DISKS=$(sysctl -n kern.disks)
for d in $DISKS; do
	echo $d | grep -q "^nvd"
	if [ $? -eq 0 ] ; then
		NVME="YES"
		NVMECTL=$(echo $d | sed 's|nvd|nvme|g')
	else
		NVME="NO"
	fi

	DISKNAME=$(cat $DMESG | grep "^$d:" | head -n 1 | cut -d "<" -f 2 | cut -d ">" -f 1)
	add_to_json_obj "hardware.storage.${d}" "name" "$DISKNAME"
	DISKSN=$(diskinfo -s $d)
	add_to_json_obj "hardware.storage.${d}" "serial" "$DISKSN"
	DISKSIZE=$(diskinfo $d | awk '{print $3}')
	add_to_json_obj "hardware.storage.${d}.size" "raw" "$DISKSIZE"
	DISKSIZEKB=$(expr $DISKSIZE / 1024)
	add_to_json_obj "hardware.storage.${d}.size" "kb" "$DISKSIZEKB"
	DISKSIZEMB=$(expr $DISKSIZEKB / 1024)
	add_to_json_obj "hardware.storage.${d}.size" "mb" "$DISKSIZEMB"
	if [ -e "/usr/local/sbin/smartctl" -a "$NVME" != "YES" ] ; then
		smartctl -a /dev/${d} >${TMPFILE}.smart
		DISKFAM=$(cat ${TMPFILE}.smart | grep 'Model Family:' | cut -d ':' -f 2 | tr -s ' ')
		add_to_json_obj "hardware.storage.${d}" "family" "$DISKFAM"
		DISKFIRM=$(cat ${TMPFILE}.smart | grep 'Firmware Version:' | cut -d ':' -f 2 | tr -s ' ')
		add_to_json_obj "hardware.storage.${d}" "firmware" "$DISKFIRM"
		DISKROTATE=$(cat ${TMPFILE}.smart | grep 'Rotation Rate:' | cut -d ':' -f 2 | tr -s ' ')
		add_to_json_obj "hardware.storage.${d}" "rotation" "$DISKROTATE"
		rm ${TMPFILE}.smart
	fi

	if [ $NVME = "YES" ] ; then
		add_to_json_obj "hardware.storage.${d}" "type" "NVME"
		nvmecontrol identify $NVMECTL >${TMPFILE}.smart
		DISKFAM=$(cat ${TMPFILE}.smart | grep 'Model Number:' | cut -d ':' -f 2 | tr -s ' ')
		add_to_json_obj "hardware.storage.${d}" "family" "$DISKFAM"
		DISKFIRM=$(cat ${TMPFILE}.smart | grep 'Firmware Version:' | cut -d ':' -f 2 | tr -s ' ')
		add_to_json_obj "hardware.storage.${d}" "firmware" "$DISKFIRM"
		rm ${TMPFILE}.smart
	else
		DISKSPEED=$(cat $DMESG | grep "^$d:" | grep "transfers" | awk '{print $2}')
		add_to_json_obj "hardware.storage.${d}" "speed" "$DISKSPEED"
		DISKCONNECTION=$(cat $DMESG | grep "^$d:" | grep "transfers" | cut -d '(' -f 2 | cut -d ')' -f 1)
		add_to_json_obj "hardware.storage.${d}" "connection" "$DISKCONNECTION"

		echo "$DISKNAME $DISKFAM $DISKROTATE" | grep -q -e "SSD" -e "Solid State"
		if [ $? -eq 0 ] ; then
			add_to_json_obj "hardware.storage.${d}" "type" "SSD"
		else
			add_to_json_obj "hardware.storage.${d}" "type" "Spinning"
		fi
	fi
done

cat ${TMPFILE}
rm ${TMPFILE}

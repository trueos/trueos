#!/bin/sh
#-
# Copyright (c) 2018 iXsystems, Inc.  All rights reserved.
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
# $FreeBSD: $

# Query mbr partitions label and display them
##############################

. ${PROGDIR}/backend/functions.sh
. ${PROGDIR}/backend/functions-disk.sh


if [ -z "${1}" ]
then
  echo "Error: No partition specified!"
  exit 1
fi

if [ ! -e "/dev/${1}" ]
then
  echo "Error: Partition /dev/${1} does not exist!"
  exit 1
fi


gpart show ${1} >/dev/null 2>/dev/null
if [ "$?" != "0" ] ; then
  # Partitons is not a primary partition
  echo "${1} is not a primary partition"
  exit
fi


SLICE_PART="${1}"
TMPDIR=${TMPDIR:-"/tmp"}

TYPE=`gpart show ${1} | awk '/^=>/ { printf("%s",$5); }'`
echo "${1}-format: $TYPE"

# Set some search flags
PART="0"
EXTENDED="0"
START="0"
SIZEB="0"

# Get a listing of partitions from the primary partition
get_partitions_lables "${SLICE_PART}"
LABELS="${VAL}"
for curpart in $LABELS
do

  # First get the sysid / label for this partition
  get_partition_label "${SLICE_PART}" "${curpart}"
  echo "${curpart}-sysid: ${VAL}"
  echo "${curpart}-label: ${VAL}"

  # Now get the startblock, blocksize and MB size of this partition
  get_label_startblock "${SLICE_PART}" "${curpart}"
  START="${VAL}"
  echo "${curpart}-blockstart: ${START}"

  get_label_blocksize "${SLICE_PART}" "${curpart}"
  SIZEB="${VAL}"
  echo "${curpart}-blocksize: ${SIZEB}"

  SIZEMB=$(convert_blocks_to_megabyte ${SIZEB})
  echo "${curpart}-sizemb: ${SIZEMB}"

done


# Now calculate any free space
FREEB=`gpart show ${SLICE_PART} | grep '\- free\ -' | awk '{print $2}' | sort -g | tail -1`
FREEMB="`expr ${FREEB} / 2048`"
echo "${1}-freemb: $FREEMB"
echo "${1}-freeblocks: $FREEB"


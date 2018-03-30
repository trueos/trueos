#!/bin/sh
#-
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2010 iXsystems, Inc.  All rights reserved.
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

# Functions which perform the mounting / unmount for upgrades

. ${PROGDIR}/backend/functions-unmount.sh

# Mount the target upgrade partitions
mount_zpool_upgrade()
{

  # Lets start by importing the specified zpool
  rc_halt "zpool import -f -R ${FSMNT} -N ${ZPOOLCUSTOMNAME}"

  # Before we create a new BE, we are going to mount the most recent BE and
  # take a look around. If this was using GRUB boot-loader, we have to save some
  # files
  newestBE=`zfs list -H -r ${ZPOOLCUSTOMNAME}/ROOT | tail -n 1 | awk '{print $1}'`
  rc_halt "mount -t zfs ${newestBE} ${FSMNT}"
  if [ -e "${FSMNT}/boot/grub/grub.cfg" ] ; then
    # We have a GRUB installation, save some files
    tar cvf ${TMPDIR}/GRUB-cfg.tar -C ${FSMNT} ./usr/local/etc/default/grub ./usr/local/etc/beadm.conf ./boot/efi
    export FORCEPKGINSTALLGRUB="YES"
  fi
  rc_halt "umount ${FSMNT}"

  # Lets create a new BE now
  BEDATASET="${ZPOOLCUSTOMNAME}/ROOT/`uname -r`-`date +%Y%m%d%H%M`"
  rc_halt "zfs create -o canmount=noauto ${BEDATASET}"

  rc_halt "mount -t zfs ${BEDATASET} ${FSMNT}"

};

# Function which unmounts all the mounted file-systems
unmount_upgrade()
{
  cd /

  # Activate this new BE
  if [ -e "/root/beadm.install" ] ; then
     rc_halt "mount -t devfs devfs ${FSMNT}/dev"
     rc_halt "cp /root/beadm.install ${FSMNT}/root/beadm.install"
     rc_halt "chmod 755 ${FSMNT}/root/beadm.install"

     # Check if we need to restamp grub
     if [ -e "${TMPDIR}/GRUB-cfg.tar" ] ; then
       rc_halt "tar xvpf ${TMPDIR}/GRUB-cfg.tar -C ${FSMNT}"

       # Change any pcbsd -> trueos references
       sed -i '' 's|pcbsd|trueos|g' ${FSMNT}/usr/local/etc/default/grub

       rc_halt "chroot ${FSMNT} grub-mkconfig -o /boot/grub/grub.cfg"
     fi

     rc_halt "chroot ${FSMNT} /root/beadm.install activate `basename ${BEDATASET}`"
     rc_halt "rm ${FSMNT}/root/beadm.install"
     rc_halt "umount -f ${FSMNT}/dev"
  fi

  # Copy zpool.cache
  if [ -e "/boot/zfs/zpool.cache" ] ; then
   rc_halt "cp /boot/zfs/zpool.cache ${FSMNT}/boot/zfs/"
  fi

  # Unmount FS
  umount_all_dir "${FSMNT}"

  rc_nohalt "umount -f ${FSMNT}"
};

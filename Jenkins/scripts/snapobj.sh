#!/bin/sh
#
# Helper script to take or roll-back a snapshot of /usr/obj/foo
# build directories so we can do incremental builds on our Jenkins hosts

# Get the default zpool
ZPOOL=$(mount | grep 'on / ' | cut -d '/' -f 1)

prepare() {
	mount | grep -q "on $1 "
	if [ $? -ne 0 ] ; then
		# Create the dataset
		zfs create -p -o mountpoint=$1 ${ZPOOL}$1
		if [ $? -ne 0 ] ; then
			exit 1
		fi
		# Create the initial snapshot
		zfs snapshot ${ZPOOL}${1}@clean
		if [ $? -ne 0 ] ; then
			exit 1
		fi
	fi
}

snapshot() {
	# Cleanup old snapshot (if it exists)
	zfs destroy ${ZPOOL}${1}@clean

	# Create the snapshot
	zfs snapshot ${ZPOOL}${1}@clean
	if [ $? -ne 0 ] ; then
		exit 1
	fi
}

rollback() {
	# Rollback to pristine state snapshot
	zfs rollback ${ZPOOL}${1}@clean
	if [ $? -ne 0 ] ; then
		exit 1
	fi
}

# Make sure datasets are all in place
prepare "$2"

case $1 in
	snapshot) snapshot "$2" ;;
	rollback) rollback "$2" ;;
	*) echo "Invalid option specified: $1"
		exit 1
		;;
esac


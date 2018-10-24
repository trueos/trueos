#!/bin/sh

LPKG="$1"
SRCDIR="$2"
PLIST="$3"
MANIFEST="$4"

PATH="${PATH}:/usr/local/bin:/usr/local/sbin"
export PATH

if [ ! -e "$MANIFEST" ] ; then
	echo "Missing MANIFEST"
	exit 1
fi

# Verify we have anything to strip in our MANIFEST
if [ "$(jq -r '."base-packages"."strip-plist" | length' $MANIFEST)" = "0" ] ; then
	return 0
fi

# Loop through and remove specified files from plist (if found)
for strip in $(jq -r '."base-packages"."strip-plist" | join(" ")' $MANIFEST)
do
	grep -q " $strip" "${PLIST}"
	if [ $? -ne 0 ] ; then continue ; fi

	echo "====> Removing $strip from $LPKG - $PLIST" >&2

	cat "${PLIST}" | grep -v " $strip" > "${PLIST}.new"
	mv "${PLIST}.new" "${PLIST}"
done

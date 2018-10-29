#!/bin/sh

LPKG="$1"
WORLDDIR="$2"
PLIST="$3"
UCL="$4"

PATH="${PATH}:/usr/local/bin:/usr/local/sbin"
export PATH

echo "====> Calculating plist checksum for $LPKG"

# Get the checksum of the plist itself, check for permissions
# or other ownerships changing, even if the file contents
# themselves didn't
CSUMTOTAL="$(sha256 -q ${PLIST})"

# Add the new depends to the UCL file
while read JLINE
do
	# We can skip directories, covered by the whole
	# plist checksum
	echo "$JLINE" | grep -q "^@dir"
	if [ $? -eq 0 ] ; then continue ; fi

	# Get the filename
	JFILE=$(echo $JLINE | awk '{print $2}')

	if [ ! -e "${WORLDDIR}/${JFILE}" ] ; then
		echo "ERROR: Missing ${WORLDDIR}/${JFILE} from plist"
		exit 1
	fi

	# Get the checksum of this particular file
	CSUM="$(sha256 -q ${WORLDDIR}/${JFILE})"

	# Add it to the total string
	CSUMTOTAL="${CSUMTOTAL}${CSUM}"
	
done < ${PLIST}

# Save the resulting sha256 checksum of this plists contents
FINALCSUM=$(echo "${CSUMTOTAL}" | sha256 -q)
echo "${FINALCSUM}" > ${PLIST}.csum

# Save this resulting checksum to the package annotations for later
# consumption
cat << EOF >>${UCL}
annotations: {
  "psum": "${FINALCSUM}"
}
EOF

#!/bin/sh

LPKG="$1"
SRCDIR="$2"
UCL="$3"
MANIFEST="$4"

if [ ! -e "$MANIFEST" ] ; then
	echo "Missing MANIFEST"
	exit 1
fi

if [ ! -e "/usr/local/bin/uclcmd" ] ; then
  echo "Please install missing \"uclcmd\""
  exit 1
fi

# Verify we have depends in our MANIFEST
if [ "$(jq -r '."base-packages"."depends"."'$LPKG'"' $MANIFEST)" = "null" ] ; then
	return 0
fi

echo "====> Injecting depends into \"$LPKG\" via $UCL"

# Add the new depends to the UCL file
for pkg in $(jq -r '."base-packages"."depends"."'$LPKG'" | keys[]' $MANIFEST)
do
	# Get the origin / version for this new depend
	origin=$(jq -r '."base-packages"."depends"."'$LPKG'"."'$pkg'"."origin"' $MANIFEST)
	version=$(jq -r '."base-packages"."depends"."'$LPKG'"."'$pkg'"."version"' $MANIFEST)

        echo "=====> Injecting \"$origin\" into \"$LPKG\""

	# Transform the UCL file and insert the new depend
	/usr/local/bin/uclcmd get --file ${UCL} -j '.' | jq -r '."deps" |= .+ { "'$pkg'":{"origin":"'$origin'","version":"'$version'"} }' > ${UCL}.new
	if [ $? -ne 0 ] ; then return 1; fi

	# Convert back to UCL
	/usr/local/bin/uclcmd get --file ${UCL}.new -u '.' > ${UCL}
	if [ $? -ne 0 ] ; then return 1; fi
	rm ${UCL}.new
	if [ $? -ne 0 ] ; then return 1; fi
done

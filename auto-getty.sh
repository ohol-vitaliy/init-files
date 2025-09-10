#!/usr/bin/env sh

get_command() {
	PATH="/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin" command -v "$1"
}

ECHO="$(get_command echo)"
AGETTY="$(get_command agetty)"
GETTY="$(get_command getty)"

if [ -n "$AGETTY" ]; then
	exec $AGETTY --noclear -8 -s 38400 "$1" linux
elif [ -n "$GETTY" ]; then
	exec $GETTY 38400 "$1" linux
else
	$ECHO "No getty or agetty found"
	exit 1
fi

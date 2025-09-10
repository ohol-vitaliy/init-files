#!/usr/bin/env sh

GREP="$(command -v grep)"
HEAD="$(command -v head)"
ECHO="echo"
SYNC="sync"
OD="od"
CUT="cut"
XARGS="xargs"
LOGGER="logger -t $0"

DEV=$($GREP -E -A5 "Lid Switch" /proc/bus/input/devices | $GREP -o "event[0-9]*" | $HEAD -n1)

[ -z "$DEV" ] && {
	$LOGGER -p user.err "No lid device found"
	exit 1
}

DEV_PATH="/dev/input/$DEV"
unset DEV
$ECHO "Monitoring $DEV_PATH for lid close..."

while true; do
	line=$($OD -An -tx2 -w24 -N24 "$DEV_PATH" | $XARGS | $CUT -d" " -f9-11)
	$LOGGER "Got $line"

	if [ "$line" = "0005 0000 0001" ]; then
		$LOGGER "Lid closed -> suspend"
		$SYNC
		$ECHO mem >/sys/power/state
	fi
done
unset line

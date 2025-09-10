#!/usr/bin/env sh

GREP="/usr/bin/grep"
HEAD="/usr/bin/head"
ECHO="/usr/bin/echo"
SYNC="/usr/bin/sync"
OD="/usr/bin/od"
CUT="/usr/bin/cut"
XARGS="/usr/bin/xargs"
LOGGER="/usr/bin/logger -e -t $0"

DEV=$($GREP -E -A5 "Power Button" /proc/bus/input/devices | $GREP -o "event[0-9]*" | $HEAD -n1)

[ -z "$DEV" ] && {
	$LOGGER -p user.err "No power button device found"
	exit 1
}

DEV_PATH="/dev/input/$DEV"
unset DEV
$LOGGER "Monitoring $DEV_PATH power button press..."

while true; do
	line=$($OD -An -tx2 -w24 -N24 "$DEV_PATH" | $XARGS | $CUT -d" " -f9-11)
	$LOGGER "Got $line"

	if [ "$line" = "0001 0074 0001" ]; then
		$LOGGER "Button pressed -> suspend"
		$SYNC
		$ECHO mem >/sys/power/state
	fi
done
unset line

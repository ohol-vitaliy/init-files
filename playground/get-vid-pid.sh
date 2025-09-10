#!/usr/bin/env sh

get_usb_id() {
	devnode="$1"
	bus=$(basename "$(dirname "$devnode")" | sed 's/^0*//')
	dev=$(basename "$devnode" | sed 's/^0*//')

	for d in /sys/bus/usb/devices/*; do
		[ -f "$d/busnum" ] || continue
		[ -f "$d/devnum" ] || continue

		if [ "$(cat "$d/busnum")" = "$bus" ] && [ "$(cat "$d/devnum")" = "$dev" ]; then
			vid=$(cat "$d/idVendor")
			pid=$(cat "$d/idProduct")
			echo "${vid}:${pid}"
			return 0
		fi
	done
	echo "not found" >&2
	return 1
}

# DEVNODE="/dev/bus/usb/001/001"
get_usb_id "$1"
# BUS=$(basename "$(dirname "$DEVNODE")" | sed 's/^0*//')
# DEV=$(basename "$DEVNODE" | sed 's/^0*//')

# for d in /sys/bus/usb/devices/*; do
#   [ -f "$d/busnum" ] || continue
#   [ -f "$d/devnum" ] || continue
#   if [ "$(cat "$d/busnum")" -eq "$BUS" ] && [ "$(cat "$d/devnum")" -eq "$DEV" ]; then
#     printf "%s:%s\n" "$(cat "$d/idVendor" 2>/dev/null)" "$(cat "$d/idProduct" 2>/dev/null)"
#     exit 0
#   fi
# done

# echo "device not found in sysfs" >&2
# exit 1

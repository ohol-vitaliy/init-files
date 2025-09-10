#!/usr/bin/env sh

get_command() {
	PATH="/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin" command -v "$1"
}

LN="$(get_command ln)"
LOGGER="$(get_command logger)"
PRINTF="$(get_command printf)"
READLINK="$(get_command readlink)"
RM="$(get_command rm)"

set +x

[ -n "$MDEV" ] || exit 0
[ -n "$DEVNAME" ] || exit 0
[ "$SUBSYSTEM" = "input" ] || exit 0

MDEV=${MDEV##*/}
BPATH="input"
SPATH="/sys$DEVPATH"
NAME=""
LINKS="mouse trackpoint kbd extrabuttons"

if [ -e "$SPATH" ]; then
	read -r NAME <"$SPATH/../name"
fi

$LOGGER "$($PRINTF "bpath = '%s'; spath = '%s'; name = '%s';\n" "$BPATH" "$SPATH" "$NAME")"

cd $BPATH || exit 0
case "$ACTION" in
add | "")
	case "$NAME" in
	"SynPS/2 Synaptics TouchPad"|"Elan Touchpad")
		$LN -fs "$MDEV" mouse
		;;
	"TPPS/2 IBM TrackPoint")
		$LN -fs "$MDEV" trackpoint
		;;
	"AT Translated Set 2 keyboard")
		$LN -fs "$MDEV" kbd
		;;
	"ThinkPad Extra Buttons")
		$LN -fs "$MDEV" extrabuttons
		;;
	esac
	;;
remove)
	# sysfs entry is already gone, so match links by their target instead of NAME
	for link in $LINKS; do
		[ "$($READLINK "$link")" = "$MDEV" ] && $RM -f "$link"
	done
	;;
esac

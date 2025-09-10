#!/usr/bin/env sh

get_command() {
	PATH="/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin" command -v "$1"
}

BASENAME="$(get_command basename)"
CAT="$(get_command cat)"
LN="$(get_command ln)"
LOGGER="$(get_command logger)"
PRINTF="$(get_command printf)"
RM="$(get_command rm)"

set +x

[ -n "$MDEV" ] || exit 0
[ -n "$DEVNAME" ] || exit 0
[ "$SUBSYSTEM" = "input" ] || exit 0

MDEV=$($BASENAME "$MDEV")
BPATH="input"
SPATH="/sys$DEVPATH"
NAME=""

if [ -e "$SPATH" ]; then
	NAME=$($CAT "$SPATH/../name")
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
	case "$NAME" in
	"SynPS/2 Synaptics TouchPad"|"Elan Touchpad")
		$RM -f mouse
		;;
	"TPPS/2 IBM TrackPoint")
		$RM -f trackpoint
		;;
	"AT Translated Set 2 keyboard")
		$RM -f kbd
		;;
	"ThinkPad Extra Buttons")
		$RM -f extrabuttons
		;;
	esac
	;;
esac

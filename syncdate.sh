#!/usr/bin/env sh

get_command() {
	PATH="/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin" command -v "$1"
}

LOGGER="$(get_command logger) -t $0"
NTP_SERVER="pool.ntp.org"
$LOGGER "Syncing time..."

SNTP="$(get_command sntp)"
NTPDATE="$(get_command ntpdate)"
BUSYBOX="$(get_command busybox)"
CURL="$(get_command curl)"
GREP="$(get_command grep)"
CUT="$(get_command cut)"
DATE="$(get_command date)"

if [ -x "$SNTP" ]; then
	$SNTP -s "$NTP_SERVER" 2>/dev/null
elif [ -x "$NTPDATE" ]; then
	$NTPDATE -u "$NTP_SERVER" 2>/dev/null
elif [ -x "$BUSYBOX" ]; then
	$BUSYBOX ntpd -q -p "$NTP_SERVER" 2>/dev/null
else
	$DATE -s "$($CURL -sI google.com | $GREP -i '^date:' | $CUT -d' ' -f2-)"
fi

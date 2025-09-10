#!/usr/bin/env sh

get_command() {
	PATH="/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin" command -v "$1"
}

LOGGER="$(get_command logger) -t $0"
NTP_SERVERS="0.pool.ntp.org 1.pool.ntp.org 2.pool.ntp.org time1.google.com time2.google.com time3.google.com time4.google.com time.aws.com time.cloudflare.com"
$LOGGER "Syncing time..."

SNTP="$(get_command sntp)"
NTPDATE="$(get_command ntpdate)"
BUSYBOX="$(get_command busybox)"
CURL="$(get_command curl)"
GREP="$(get_command grep)"
CUT="$(get_command cut)"
DATE="$(get_command date)"

# shellcheck disable=SC2086 # word-split the server list on purpose
if [ -x "$SNTP" ]; then
	$SNTP -s $NTP_SERVERS 2>/dev/null
elif [ -x "$NTPDATE" ]; then
	$NTPDATE -u $NTP_SERVERS 2>/dev/null
elif [ -x "$BUSYBOX" ]; then
	set --
	for server in $NTP_SERVERS; do
		set -- "$@" -p "$server"
	done
	$BUSYBOX ntpd -q "$@" 2>/dev/null
else
	$DATE -s "$($CURL -sI google.com | $GREP -i '^date:' | $CUT -d' ' -f2-)"
fi

#!/usr/bin/env sh

get_command() {
	PATH="/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin" command -v "$1"
}

LOGGER="$(get_command logger) -t $0"
NTP_SERVER1="0.pool.ntp.org"
NTP_SERVER2="1.pool.ntp.org"
NTP_SERVER3="2.pool.ntp.org"
NTP_SERVER4="time1.google.com"
NTP_SERVER5="time2.google.com"
NTP_SERVER6="time3.google.com"
NTP_SERVER7="time4.google.com"
NTP_SERVER8="time.aws.com"
NTP_SERVER9="time.cloudflare.com"
$LOGGER "Syncing time..."

SNTP="$(get_command sntp)"
NTPDATE="$(get_command ntpdate)"
BUSYBOX="$(get_command busybox)"
CURL="$(get_command curl)"
GREP="$(get_command grep)"
CUT="$(get_command cut)"
DATE="$(get_command date)"

if [ -x "$SNTP" ]; then
	$SNTP -s "$NTP_SERVER1" "$NTP_SERVER2" "$NTP_SERVER3" "$NTP_SERVER4" "$NTP_SERVER5" "$NTP_SERVER6" "$NTP_SERVER7" "$NTP_SERVER8" "$NTP_SERVER9" 2>/dev/null
elif [ -x "$NTPDATE" ]; then
	$NTPDATE -u "$NTP_SERVER1" "$NTP_SERVER2" "$NTP_SERVER3" "$NTP_SERVER4" "$NTP_SERVER5" "$NTP_SERVER6" "$NTP_SERVER7" "$NTP_SERVER8" "$NTP_SERVER9" 2>/dev/null
elif [ -x "$BUSYBOX" ]; then
	$BUSYBOX ntpd -q \
		-p "$NTP_SERVER1" \
		-p "$NTP_SERVER2" \
		-p "$NTP_SERVER3" \
		-p "$NTP_SERVER4" \
		-p "$NTP_SERVER5" \
		-p "$NTP_SERVER6" \
		-p "$NTP_SERVER7" \
		-p "$NTP_SERVER8" \
		-p "$NTP_SERVER9" 2>/dev/null
else
	$DATE -s "$($CURL -sI google.com | $GREP -i '^date:' | $CUT -d' ' -f2-)"
fi

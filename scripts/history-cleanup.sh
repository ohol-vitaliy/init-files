#!/bin/sh

URL_FILTER=$(
	cat <<EOF | base64 -d
V0hFUkUgdXJsIExJS0UgJyVzcGFua2JhbmclJwpPUiB1cmwgTElLRSAnJXRuYWZsaXglJwpPUiB1
cmwgTElLRSAnJWhlbnRhaSUnCk9SIHVybCBMSUtFICclcG9ybiUnCk9SIHVybCBMSUtFICclam95
cmVhY3RvciUnCk9SIHVybCBMSUtFICclcmVkZ2lmJScKT1IgdXJsIExJS0UgJyVpbnN0YWdyYW0u
Y29tJScKT1IgdXJsIExJS0UgJyVmYWNlYm9vay5jb20lJwpPUiB1cmwgTElLRSAnJWNoYXRncHQu
Y29tJScKT1IgdXJsIExJS0UgJyVjbGF1ZGUuYWklJwo=
EOF
)

OLDER_VISITS_Q="$(
	cat <<EOF
DELETE FROM moz_historyvisits
WHERE visit_date < (
    CAST(strftime('%s', 'now') AS INTEGER) - 30 * 86400
) * 1000000
EOF
)"
VISITS_Q="DELETE FROM moz_historyvisits WHERE place_id IN (SELECT id FROM moz_places $URL_FILTER)"
PLACES_Q="DELETE FROM moz_places $URL_FILTER"
DOWNLOADS_Q="$(
	cat <<EOF
DELETE FROM moz_annos
WHERE anno_attribute_id IN (
    SELECT id FROM moz_anno_attributes
    WHERE name LIKE 'downloads/%'
)
EOF
)"

HISTORY_TEMP=$(mktemp)
HISTORY="$HOME/.config/cl_history"
awk '/^[^a-zA-Z0-9.]/ || /^(sudo\s+|\.\/)?(clear|ls|la|ll|cd|lsmod|gs|ga|gd|gpl|gpsh|sus|juice|chmod|chown|crontab|decr|encr|df|du|wc|which|feh|find|free|git restore|hist|ln|powersave|smarthome|speaker|headphone|exit|ncdu|cat|fg|jobs|pacl|pacs|pacu|pacc|pstree|strip|swap|mkdir|tail|head|grep|tic|vim|unzip|zip|tar|rar|unrar|wget|xxd|pwm|sync|zathura|tree|tmux|rm|mv|cp|u?mount|mpv|yt-dlp)/{next;} {print}' "$HISTORY" | sort -u >"$HISTORY_TEMP" \
	&& mv "$HISTORY_TEMP" "$HISTORY"

ff_cleanup() {
	DB=$(find "$1" -name places.sqlite -print -quit)

	[ -f "$DB" ] || {
		echo "History database not found" >&2
		return
	}

	printf "Found: %s\n" "$DB"
	sqlite3 "$DB" "BEGIN; $OLDER_VISITS_Q; $VISITS_Q; $PLACES_Q; $DOWNLOADS_Q; COMMIT; VACUUM;"
}

ff_cleanup "$HOME/.mozilla"
ff_cleanup "$HOME/.librewolf"

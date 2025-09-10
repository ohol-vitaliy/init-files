#!/bin/sh

URL_FILTER=$(
	cat <<EOF | base64 -d
V0hFUkUgdXJsIExJS0UgJyVzcGFua2JhbmclJwpPUiB1cmwgTElLRSAnJXBvcm5odWIlJwpPUiB1
cmwgTElLRSAnJXRuYWZsaXglJwpPUiB1cmwgTElLRSAnJWpveXJlYWN0b3IlJwpPUiB1cmwgTElL
RSAnJXJlZGdpZiUnCk9SIHVybCBMSUtFICclaW5zdGFncmFtLmNvbSUnCk9SIHVybCBMSUtFICcl
ZmFjZWJvb2suY29tJScKT1IgdXJsIExJS0UgJyVjaGF0Z3B0LmNvbSUnCk9SIHVybCBMSUtFICcl
Y2xhdWRlLmFpJScK
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
awk '/^[^a-zA-Z0-9.]/ || /^(sudo\s+)?(cd|lsmod|ncdu|strip|swap|mkdir|tail|head|grep|tic|vim|unzip|zip|tar|rar|unrar|wget|xxd|zathura|tree|tmux|rm|mv|cp|u?mount)/{next;} {print}' "$HISTORY" >"$HISTORY_TEMP" &&
	mv "$HISTORY_TEMP" "$HISTORY"

ff_cleanup() {
	DB=$(find "$1" -name places.sqlite -print -quit)

	[ -f "$DB" ] || {
		echo "History database not found" >&2
		return
	}

	printf "Found: %s\n" "$DB"
	sqlite3 "$DB" "BEGIN; $OLDER_VISITS_Q; $VISITS_Q; $DOWNLOADS_Q; COMMIT;"
}

ff_cleanup "$HOME/.mozilla"
ff_cleanup "$HOME/.librewolf"

#!/bin/sh

[ -z "$1" ] && echo "Usage: $0 \"<command>\"" && exit 1
SERVICE_CMD="$1"

SAFE_NAME="$(echo "$SERVICE_CMD" | tr -cd '[:alnum:]_-')"
[ -z "$SAFE_NAME" ] && SAFE_NAME="svc_$(date +%s%N)"

PIDFILE="/tmp/${SAFE_NAME}.pid"
LOCKFILE="/tmp/${SAFE_NAME}.lock"
LOGFILE="/var/log/${SAFE_NAME}.log"
mkdir -p "$(dirname "$LOGFILE")"

# Acquire lock
exec 200>"$LOCKFILE"
flock -n 200 || exit 0

is_running() {
    if [ -f "$PIDFILE" ]; then
        PID=$(cat "$PIDFILE" 2>/dev/null)
        if [ -n "$PID" ] && [ -d "/proc/$PID" ]; then
            CMDLINE=$(tr -d '\0' </proc/$PID/cmdline 2>/dev/null)
            case "$CMDLINE" in
                *"$SERVICE_CMD"*) return 0 ;;
            esac
        fi
    fi

    for pid in $(ls /proc 2>/dev/null | grep -E '^[0-9]+$'); do
        [ "$pid" = "$$" ] && continue
        CMDLINE=$(tr -d '\0' </proc/$pid/cmdline 2>/dev/null)
        case "$CMDLINE" in
            *"$SERVICE_CMD"*) return 0 ;;
        esac
    done
    return 1
}

if ! is_running; then
    echo "[$(date)] Starting: $SERVICE_CMD" >>"$LOGFILE"
    nohup $SERVICE_CMD >>"$LOGFILE" 2>&1 &
    echo $! > "$PIDFILE"
fi


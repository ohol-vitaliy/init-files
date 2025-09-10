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

# Scan /proc for the daemonized process
find_pid() {
    for pid in $(ls /proc 2>/dev/null | grep -E '^[0-9]+$'); do
        [ "$pid" = "$$" ] && continue
        [ ! -d "/proc/$pid" ] && continue
        CMDLINE=$(tr -d '\0' </proc/$pid/cmdline 2>/dev/null)
        case "$CMDLINE" in
            *"$SERVICE_CMD"*) echo "$pid"; return ;;
        esac
    done
    return
}

PID=$(find_pid)

if [ -n "$PID" ]; then
    echo $PID > "$PIDFILE"  # update PID file to the real running PID
else
    # start the daemon
    echo "[$(date)] Starting daemon: $SERVICE_CMD" >>"$LOGFILE"
    nohup $SERVICE_CMD >>"$LOGFILE" 2>&1 &
    # wait a second to let it daemonize
    sleep 1
    # find the actual child
    PID=$(find_pid)
    [ -n "$PID" ] && echo $PID > "$PIDFILE"
fi


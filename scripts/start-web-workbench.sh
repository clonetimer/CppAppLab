#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BINARY="${CPPAPPLAB_BINARY:-$ROOT/build/cppapplab}"
HOST="127.0.0.1"
PORT="${CPPAPPLAB_WEB_PORT:-8765}"
RUNTIME_DIR="${XDG_RUNTIME_DIR:-${TMPDIR:-/tmp}}"
PID_FILE="$RUNTIME_DIR/cppapplab-web-${UID}.pid"
LOG_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/cppapplab"
LOG_FILE="$LOG_DIR/web-workbench.log"

usage() {
    cat <<'EOF'
Usage:
  bash scripts/start-web-workbench.sh [start|stop|status]

Starts the local CppAppLab browser workbench on 127.0.0.1:8765.
Set CPPAPPLAB_WEB_PORT to use another local port.
EOF
}

is_our_process() {
    local pid="$1"
    local command_line
    [[ -r "/proc/$pid/cmdline" ]] || return 1
    command_line="$(tr '\0' ' ' < "/proc/$pid/cmdline" 2>/dev/null || true)"
    [[ "$command_line" == *"$ROOT/web/server/cppapplab_web.py"* ]]
}

health_check() {
    python3 - "$HOST" "$PORT" <<'PY'
import json
import sys
import urllib.error
import urllib.request

url = f"http://{sys.argv[1]}:{sys.argv[2]}/api/v1/health"
try:
    with urllib.request.urlopen(url, timeout=1.5) as response:
        if response.status != 200:
            raise SystemExit(1)
        health = json.load(response)
        if (
            health.get("status") != "ok"
            or not isinstance(health.get("engine"), str)
            or "CppAppLab" not in health["engine"]
            or not isinstance(health.get("task_count"), int)
            or health["task_count"] < 1
        ):
            raise SystemExit(1)
        print(json.dumps(health))
except (OSError, urllib.error.URLError):
    raise SystemExit(1)
PY
}

stop_service() {
    if [[ ! -f "$PID_FILE" ]]; then
        echo "[INFO] CppAppLab Web Workbench is not running."
        return 0
    fi
    local pid
    pid="$(cat "$PID_FILE")"
    if [[ ! "$pid" =~ ^[0-9]+$ ]] || ! kill -0 "$pid" 2>/dev/null || ! is_our_process "$pid"; then
        rm -f "$PID_FILE"
        echo "[INFO] Removed a stale service PID file without signalling an unrelated process."
        return 0
    fi
    kill "$pid"
    for _ in {1..30}; do
        if ! kill -0 "$pid" 2>/dev/null; then
            rm -f "$PID_FILE"
            echo "[PASS] CppAppLab Web Workbench stopped."
            return 0
        fi
        sleep 0.2
    done
    echo "[FAIL] The web service did not stop within six seconds." >&2
    return 1
}

open_browser() {
    local url="http://$HOST:$PORT/"
    if [[ "${CPPAPPLAB_OPEN_BROWSER:-0}" != "1" ]]; then
        return 0
    fi
    if command -v cmd.exe >/dev/null 2>&1 && cmd.exe /c start "" "$url" >/dev/null 2>&1; then
        echo "[PASS] Opened the workbench in the Windows default browser."
        return 0
    fi
    if command -v wslview >/dev/null 2>&1 && wslview "$url" >/dev/null 2>&1; then
        echo "[PASS] Opened the workbench with wslview."
        return 0
    fi
    echo "[INFO] Browser auto-open is unavailable. Open this address manually: $url"
    return 0
}

status_service() {
    if health_check >/dev/null 2>&1; then
        echo "[PASS] CppAppLab Web Workbench is ready: http://$HOST:$PORT/"
        health_check
        return 0
    fi
    echo "[INFO] CppAppLab Web Workbench is not responding on $HOST:$PORT."
    return 1
}

start_service() {
    if [[ ! -x "$BINARY" ]]; then
        echo "[FAIL] CppAppLab executable not found: $BINARY" >&2
        echo "Build the existing project first, or set CPPAPPLAB_BINARY." >&2
        return 2
    fi
    if [[ ! -f "$ROOT/web/ui/index.html" || ! -f "$ROOT/web/server/cppapplab_web.py" ]]; then
        echo "[FAIL] Browser workbench files are missing from $ROOT." >&2
        return 2
    fi
    if health_check >/dev/null 2>&1; then
        echo "[PASS] CppAppLab Web Workbench is already running: http://$HOST:$PORT/"
        open_browser
        return 0
    fi

    mkdir -p "$LOG_DIR" "$RUNTIME_DIR"
    if [[ -f "$PID_FILE" ]]; then
        local old_pid
        old_pid="$(cat "$PID_FILE")"
        if [[ "$old_pid" =~ ^[0-9]+$ ]] && kill -0 "$old_pid" 2>/dev/null && is_our_process "$old_pid"; then
            echo "[FAIL] PID $old_pid exists but the service is not healthy. Inspect $LOG_FILE." >&2
            return 1
        fi
        rm -f "$PID_FILE"
    fi

    nohup python3 "$ROOT/web/server/cppapplab_web.py" \
        --root "$ROOT" \
        --cppapplab "$BINARY" \
        --host "$HOST" \
        --port "$PORT" \
        >>"$LOG_FILE" 2>&1 </dev/null &
    local pid=$!
    printf '%s\n' "$pid" > "$PID_FILE"

    for _ in {1..40}; do
        if health_check >/dev/null 2>&1; then
            echo "[PASS] CppAppLab Web Workbench is ready."
            echo "Open: http://$HOST:$PORT/"
            echo "Log:  $LOG_FILE"
            open_browser
            return 0
        fi
        if ! kill -0 "$pid" 2>/dev/null; then
            rm -f "$PID_FILE"
            echo "[FAIL] The web service exited during startup. See $LOG_FILE." >&2
            tail -n 40 "$LOG_FILE" >&2 || true
            return 1
        fi
        sleep 0.25
    done

    echo "[FAIL] The web service did not become ready within ten seconds. See $LOG_FILE." >&2
    echo "[INFO] If port $PORT is occupied, retry with CPPAPPLAB_WEB_PORT set to an unused local port." >&2
    return 1
}

case "${1:-start}" in
    start) start_service ;;
    stop) stop_service ;;
    status) status_service ;;
    -h|--help|help) usage ;;
    *) usage >&2; exit 2 ;;
esac

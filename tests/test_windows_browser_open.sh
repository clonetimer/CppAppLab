#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEMP_ROOT="$(mktemp -d)"
HEALTH_PID=""
cleanup() {
    if [[ -n "$HEALTH_PID" ]]; then
        kill "$HEALTH_PID" 2>/dev/null || true
        wait "$HEALTH_PID" 2>/dev/null || true
    fi
    rm -rf "$TEMP_ROOT"
}
trap cleanup EXIT

TEST_ROOT="$TEMP_ROOT/repo"
BIN="$TEMP_ROOT/bin"
HEALTH_FILE="$TEMP_ROOT/health.json"
PORT_FILE="$TEMP_ROOT/health-port"
mkdir -p "$TEST_ROOT/scripts" "$TEST_ROOT/build" "$TEST_ROOT/web/ui" "$TEST_ROOT/web/server" "$BIN"
cp "$ROOT/scripts/start-web-workbench.sh" "$TEST_ROOT/scripts/start-web-workbench.sh"
: > "$TEST_ROOT/web/ui/index.html"
: > "$TEST_ROOT/web/server/cppapplab_web.py"
printf '#!/usr/bin/env bash\nexit 0\n' > "$TEST_ROOT/build/cppapplab"
chmod +x "$TEST_ROOT/build/cppapplab"

cat > "$TEMP_ROOT/health_server.py" <<'PY'
import pathlib
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer

health_file = pathlib.Path(sys.argv[1])
port_file = pathlib.Path(sys.argv[2])

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path != "/api/v1/health":
            self.send_error(404)
            return
        body = health_file.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_args):
        pass

server = HTTPServer(("127.0.0.1", 0), Handler)
port_file.write_text(str(server.server_port), encoding="ascii")
server.serve_forever()
PY

cat > "$BIN/cmd.exe" <<'CMD'
#!/usr/bin/env bash
printf '%s\n' "$@" > "$CPPAPPLAB_TEST_WINDOWS_ARGS"
exit "${CPPAPPLAB_TEST_CMD_EXIT:-0}"
CMD
cat > "$BIN/wslview" <<'WSL'
#!/usr/bin/env bash
printf '%s\n' "$@" > "$CPPAPPLAB_TEST_WSLVIEW_ARGS"
exit 0
WSL
cat > "$BIN/nohup" <<'NOHUP'
#!/usr/bin/env bash
# Invalid/foreign health must not launch an actual CppAppLab server in this test.
exit 0
NOHUP
chmod +x "$BIN/cmd.exe" "$BIN/wslview" "$BIN/nohup"

printf '{"status":"ok","engine":"CppAppLab - C++ learning and engineering practice","task_count":23}\n' > "$HEALTH_FILE"
python3 "$TEMP_ROOT/health_server.py" "$HEALTH_FILE" "$PORT_FILE" &
HEALTH_PID=$!
for _ in {1..50}; do
    [[ -s "$PORT_FILE" ]] && break
    sleep 0.05
done
test -s "$PORT_FILE"
PORT="$(cat "$PORT_FILE")"

export PATH="$BIN:$PATH"
export CPPAPPLAB_TEST_WINDOWS_ARGS="$TEMP_ROOT/windows-args"
export CPPAPPLAB_TEST_WSLVIEW_ARGS="$TEMP_ROOT/wslview-args"
export CPPAPPLAB_OPEN_BROWSER=1
export CPPAPPLAB_WEB_PORT="$PORT"
export XDG_RUNTIME_DIR="$TEMP_ROOT/runtime"
export XDG_STATE_HOME="$TEMP_ROOT/state"
mkdir -p "$XDG_RUNTIME_DIR"

bash "$TEST_ROOT/scripts/start-web-workbench.sh" start >"$TEMP_ROOT/windows.log"
grep -Fxq '/c' "$CPPAPPLAB_TEST_WINDOWS_ARGS"
grep -Fxq 'start' "$CPPAPPLAB_TEST_WINDOWS_ARGS"
grep -Fxq '' "$CPPAPPLAB_TEST_WINDOWS_ARGS"
grep -Fxq "http://127.0.0.1:$PORT/" "$CPPAPPLAB_TEST_WINDOWS_ARGS"
test ! -e "$CPPAPPLAB_TEST_WSLVIEW_ARGS"

rm -f "$CPPAPPLAB_TEST_WINDOWS_ARGS"
CPPAPPLAB_TEST_CMD_EXIT=1 bash "$TEST_ROOT/scripts/start-web-workbench.sh" start >"$TEMP_ROOT/fallback.log"
grep -Fxq "http://127.0.0.1:$PORT/" "$CPPAPPLAB_TEST_WSLVIEW_ARGS"

rm -f "$CPPAPPLAB_TEST_WINDOWS_ARGS" "$CPPAPPLAB_TEST_WSLVIEW_ARGS"
CPPAPPLAB_OPEN_BROWSER=0 bash "$TEST_ROOT/scripts/start-web-workbench.sh" start >"$TEMP_ROOT/manual.log"
test ! -e "$CPPAPPLAB_TEST_WINDOWS_ARGS"
test ! -e "$CPPAPPLAB_TEST_WSLVIEW_ARGS"

# A generic HTTP 200 endpoint must not be mistaken for the CppAppLab workbench.
printf '{"status":"ok","engine":"Unrelated service","task_count":23}\n' > "$HEALTH_FILE"
if bash "$TEST_ROOT/scripts/start-web-workbench.sh" start >"$TEMP_ROOT/foreign.log" 2>&1; then
    cat "$TEMP_ROOT/foreign.log" >&2
    echo "[FAIL] An unrelated service was accepted as the CppAppLab workbench." >&2
    exit 1
fi
test ! -e "$CPPAPPLAB_TEST_WINDOWS_ARGS"
test ! -e "$CPPAPPLAB_TEST_WSLVIEW_ARGS"

echo "[PASS] Verified CppAppLab health identity, Windows browser opening, wslview fallback and manual opt-out."

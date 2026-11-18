#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TEMP_ROOT"' EXIT

TEST_ROOT="$TEMP_ROOT/repo"
BIN="$TEMP_ROOT/bin"
mkdir -p "$TEST_ROOT/scripts" "$TEST_ROOT/build" "$TEST_ROOT/web/ui" "$TEST_ROOT/web/server" "$BIN"
cp "$ROOT/scripts/start-web-workbench.sh" "$TEST_ROOT/scripts/start-web-workbench.sh"
: > "$TEST_ROOT/web/ui/index.html"
: > "$TEST_ROOT/web/server/cppapplab_web.py"
printf '#!/usr/bin/env bash\nexit 0\n' > "$TEST_ROOT/build/cppapplab"
chmod +x "$TEST_ROOT/build/cppapplab"

cat > "$BIN/python3" <<'PY'
#!/usr/bin/env bash
# Health-check calls use "-" as the Python program; server starts use a file path.
if [[ "${1:-}" == "-" ]]; then
    cat >/dev/null
    if [[ -f "$CPPAPPLAB_TEST_FOREIGN_MARKER" ]]; then
        printf '{"status":"ok","engine":"Unrelated service","task_count":23}\n'
    else
        printf '{"status":"ok","engine":"CppAppLab - C++ learning and engineering practice","task_count":23}\n'
    fi
    exit 0
fi
exit 0
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
chmod +x "$BIN/python3" "$BIN/cmd.exe" "$BIN/wslview"

export PATH="$BIN:$PATH"
export CPPAPPLAB_TEST_WINDOWS_ARGS="$TEMP_ROOT/windows-args"
export CPPAPPLAB_TEST_WSLVIEW_ARGS="$TEMP_ROOT/wslview-args"
export CPPAPPLAB_TEST_FOREIGN_MARKER="$TEMP_ROOT/foreign-health"
export CPPAPPLAB_OPEN_BROWSER=1
export CPPAPPLAB_WEB_PORT=18765
export XDG_RUNTIME_DIR="$TEMP_ROOT/runtime"
export XDG_STATE_HOME="$TEMP_ROOT/state"
mkdir -p "$XDG_RUNTIME_DIR"

bash "$TEST_ROOT/scripts/start-web-workbench.sh" start >"$TEMP_ROOT/windows.log"
grep -Fxq '/c' "$CPPAPPLAB_TEST_WINDOWS_ARGS"
grep -Fxq 'start' "$CPPAPPLAB_TEST_WINDOWS_ARGS"
grep -Fxq '' "$CPPAPPLAB_TEST_WINDOWS_ARGS"
grep -Fxq 'http://127.0.0.1:18765/' "$CPPAPPLAB_TEST_WINDOWS_ARGS"
test ! -e "$CPPAPPLAB_TEST_WSLVIEW_ARGS"

rm -f "$CPPAPPLAB_TEST_WINDOWS_ARGS"
CPPAPPLAB_TEST_CMD_EXIT=1 bash "$TEST_ROOT/scripts/start-web-workbench.sh" start >"$TEMP_ROOT/fallback.log"
grep -Fxq 'http://127.0.0.1:18765/' "$CPPAPPLAB_TEST_WSLVIEW_ARGS"

rm -f "$CPPAPPLAB_TEST_WINDOWS_ARGS" "$CPPAPPLAB_TEST_WSLVIEW_ARGS"
CPPAPPLAB_OPEN_BROWSER=0 bash "$TEST_ROOT/scripts/start-web-workbench.sh" start >"$TEMP_ROOT/manual.log"
test ! -e "$CPPAPPLAB_TEST_WINDOWS_ARGS"
test ! -e "$CPPAPPLAB_TEST_WSLVIEW_ARGS"

# A generic HTTP 200 health endpoint must not be mistaken for our workbench.
: > "$CPPAPPLAB_TEST_FOREIGN_MARKER"
if bash "$TEST_ROOT/scripts/start-web-workbench.sh" start >"$TEMP_ROOT/foreign.log" 2>&1; then
    cat "$TEMP_ROOT/foreign.log" >&2
    echo "[FAIL] An unrelated service was accepted as the CppAppLab workbench." >&2
    exit 1
fi
test ! -e "$CPPAPPLAB_TEST_WINDOWS_ARGS"
test ! -e "$CPPAPPLAB_TEST_WSLVIEW_ARGS"

echo "[PASS] Browser opens only for a verified CppAppLab health response, prefers Windows cmd.exe and falls back to wslview."

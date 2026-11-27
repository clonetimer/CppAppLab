#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ $# -ne 0 ]]; then
    echo "Usage: ./start-web-workbench (CPPAPPLAB_WEB_PORT=8765 by default)" >&2
    exit 2
fi
if ! command -v python3 >/dev/null 2>&1; then
    echo "[FAIL] Python 3 is required. Install python3 for your Ubuntu release." >&2
    exit 2
fi
if [[ ! -x "$ROOT/cppapplab" || ! -f "$ROOT/web/server/cppapplab_web.py" || ! -f "$ROOT/web/ui/index.html" ]]; then
    echo "[FAIL] The extracted learner package is incomplete." >&2
    exit 2
fi
# Foreground, independent from the source-checkout PID file. An occupied port
# fails explicitly; never reuse an arbitrary other checkout's health endpoint.
exec python3 "$ROOT/web/server/cppapplab_web.py" \
    --root "$ROOT" --cppapplab "$ROOT/cppapplab" \
    --host 127.0.0.1 --port "${CPPAPPLAB_WEB_PORT:-8765}"

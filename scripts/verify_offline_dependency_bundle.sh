#!/usr/bin/env bash
set -Eeuo pipefail

if [[ $# -ne 1 || ! -f "$1" ]]; then
    echo "Usage: $0 <offline-dependency-bundle.tar.gz>" >&2
    exit 64
fi
ARCHIVE="$1"
TEMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TEMP_ROOT"' EXIT

if [[ -f "$ARCHIVE.sha256" ]]; then
    expected="$(awk '{print $1}' "$ARCHIVE.sha256")"
    actual="$(sha256sum "$ARCHIVE" | awk '{print $1}')"
    if [[ "$expected" != "$actual" ]]; then
        echo "[FAIL] Offline bundle archive SHA-256 mismatch." >&2
        exit 1
    fi
fi

tar -xzf "$ARCHIVE" -C "$TEMP_ROOT"
BUNDLE_ROOT="$(find "$TEMP_ROOT" -mindepth 1 -maxdepth 1 -type d -name 'CppAppLab-offline-deps-*' -print -quit)"
if [[ -z "$BUNDLE_ROOT" || ! -x "$BUNDLE_ROOT/install.sh" ]]; then
    echo "[FAIL] Offline dependency bundle layout is invalid." >&2
    exit 1
fi

"$BUNDLE_ROOT/install.sh" --verify-only
"$BUNDLE_ROOT/install.sh" --verify-only --with-quality-tools
"$BUNDLE_ROOT/install.sh" --dry-run
"$BUNDLE_ROOT/install.sh" --dry-run --with-quality-tools

python3 - "$BUNDLE_ROOT/packages.lock.json" "$BUNDLE_ROOT" <<'PY'
import json
import sys
from pathlib import Path
manifest = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
root = Path(sys.argv[2])
assert manifest["target"] in ("Ubuntu 22.04 LTS amd64", "Ubuntu 24.04 LTS amd64")
assert manifest["package_installation_performed_during_build"] is False
for item in manifest["packages"]:
    deb = root / item["file"]
    assert deb.is_file(), f"Missing package: {deb}"
    assert deb.stat().st_size == item["size"], f"Size mismatch: {deb}"
print(f"[PASS] Lock manifest verified for {len(manifest['packages'])} packages.")
PY

echo "[PASS] Offline dependency bundle integrity and APT dry-run verified; no packages were installed."

#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/scripts/lib/ubuntu-support.sh"

TEMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TEMP_ROOT"' EXIT

write_release() {
    local id="$1"
    local version="$2"
    local pretty="$3"
    cat > "$TEMP_ROOT/os-release" <<EOF
ID="$id"
VERSION_ID="$version"
PRETTY_NAME="$pretty"
EOF
}

for version in 22.04 24.04; do
    write_release ubuntu "$version" "Ubuntu $version LTS"
    cppapplab_check_ubuntu "$TEMP_ROOT/os-release" > "$TEMP_ROOT/result.log"
    grep -Fq "[PASS] Ubuntu $version LTS detected" "$TEMP_ROOT/result.log"
done

for version in 20.04 24.10; do
    write_release ubuntu "$version" "Ubuntu $version"
    if cppapplab_check_ubuntu "$TEMP_ROOT/os-release" > "$TEMP_ROOT/result.log" 2>&1; then
        echo "[FAIL] Unsupported Ubuntu $version was accepted." >&2
        exit 1
    fi
    grep -Fq "support Ubuntu 22.04 LTS and Ubuntu 24.04 LTS" "$TEMP_ROOT/result.log"
done

write_release debian 12 "Debian GNU/Linux 12"
if cppapplab_check_ubuntu "$TEMP_ROOT/os-release" > "$TEMP_ROOT/result.log" 2>&1; then
    echo "[FAIL] A non-Ubuntu distribution was accepted." >&2
    exit 1
fi

echo "[PASS] Ubuntu source-build support matrix: 22.04 and 24.04 accepted; unsupported releases rejected."

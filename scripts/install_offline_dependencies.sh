#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
if [[ ! -r /etc/os-release ]]; then
    echo "[FAIL] Cannot identify the operating system." >&2
    exit 2
fi
# shellcheck disable=SC1091
source /etc/os-release
if [[ "$ID" != "ubuntu" || "$VERSION_ID" != "24.04" ]]; then
    echo "[FAIL] This dependency bundle targets Ubuntu 24.04; detected $PRETTY_NAME." >&2
    exit 2
fi
if [[ "$(dpkg --print-architecture)" != "amd64" ]]; then
    echo "[FAIL] This dependency bundle targets amd64; detected $(dpkg --print-architecture)." >&2
    exit 2
fi
if [[ ! -d "$ROOT/packages" ]] || ! compgen -G "$ROOT/packages/*.deb" >/dev/null; then
    echo "[FAIL] No local .deb files were found in $ROOT/packages." >&2
    exit 2
fi

(
    cd "$ROOT"
    sha256sum --check SHA256SUMS
)

echo "[INFO] Installing only from the bundled .deb files; network downloads are disabled."
if [[ "$EUID" -eq 0 ]]; then
    apt-get install --no-download --no-install-recommends -y "$ROOT"/packages/*.deb
else
    sudo apt-get install --no-download --no-install-recommends -y "$ROOT"/packages/*.deb
fi

for tool in g++ cmake git gdb clang-format clang-tidy; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "[FAIL] Installed dependency bundle did not provide $tool." >&2
        exit 3
    fi
done
if ! dpkg-query -W -f='${Status}' libgtest-dev 2>/dev/null | grep -q 'install ok installed'; then
    echo "[FAIL] libgtest-dev was not installed." >&2
    exit 3
fi

echo "[PASS] CppAppLab offline dependencies installed."

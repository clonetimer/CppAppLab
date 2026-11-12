#!/usr/bin/env bash
set -Eeuo pipefail

if [[ ! -r /etc/os-release ]]; then
    echo "[FAIL] Cannot identify the CI operating system." >&2
    exit 2
fi
# shellcheck disable=SC1091
source /etc/os-release
if [[ "${ID:-}" != "ubuntu" || "${VERSION_ID:-}" != "24.04" ]]; then
    echo "[FAIL] This CI mirror fallback is limited to Ubuntu 24.04." >&2
    exit 2
fi

if grep -RqlF "mirror+file:/etc/apt/apt-mirrors.txt" /etc/apt/sources.list.d 2>/dev/null; then
    echo "[INFO] Replacing the runner's Azure mirror indirection with canonical Ubuntu mirrors."
    while IFS= read -r source_file; do
        sudo mv "$source_file" "$source_file.cppapplab-disabled"
    done < <(grep -RlF "mirror+file:/etc/apt/apt-mirrors.txt" /etc/apt/sources.list.d 2>/dev/null || true)

    sudo tee /etc/apt/sources.list.d/cppapplab.sources >/dev/null <<'SOURCES'
Types: deb
URIs: http://archive.ubuntu.com/ubuntu
Suites: noble noble-updates noble-backports
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: http://security.ubuntu.com/ubuntu
Suites: noble-security
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
SOURCES
fi

sudo apt-get -o Acquire::Retries=3 update

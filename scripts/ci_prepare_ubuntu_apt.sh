#!/usr/bin/env bash
set -Eeuo pipefail

if [[ ! -r /etc/os-release ]]; then
    echo "[FAIL] Cannot identify the CI operating system." >&2
    exit 2
fi
# shellcheck disable=SC1091
source /etc/os-release
if [[ "${ID:-}" != "ubuntu" ]]; then
    echo "[FAIL] This CI mirror fallback requires Ubuntu 22.04 or 24.04." >&2
    exit 2
fi
case "${VERSION_ID:-}" in
    22.04) ubuntu_codename="jammy" ;;
    24.04) ubuntu_codename="noble" ;;
    *)
        echo "[FAIL] This CI mirror fallback supports Ubuntu 22.04 and 24.04; detected ${PRETTY_NAME:-unknown}." >&2
        exit 2
        ;;
esac

if grep -RqlF "mirror+file:/etc/apt/apt-mirrors.txt" /etc/apt/sources.list.d 2>/dev/null; then
    echo "[INFO] Replacing the runner's Azure mirror indirection with canonical Ubuntu mirrors."
    while IFS= read -r source_file; do
        sudo mv "$source_file" "$source_file.cppapplab-disabled"
    done < <(grep -RlF "mirror+file:/etc/apt/apt-mirrors.txt" /etc/apt/sources.list.d 2>/dev/null || true)

    sudo tee /etc/apt/sources.list.d/cppapplab.sources >/dev/null <<'SOURCES'
Types: deb
URIs: http://archive.ubuntu.com/ubuntu
Suites: ${ubuntu_codename} ${ubuntu_codename}-updates ${ubuntu_codename}-backports
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: http://security.ubuntu.com/ubuntu
Suites: ${ubuntu_codename}-security
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
SOURCES
fi

sudo apt-get -o Acquire::Retries=3 update

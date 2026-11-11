#!/usr/bin/env bash
set -Eeuo pipefail

if [[ ! -f /tmp/offline-dependencies.txt ]]; then
    echo "[FAIL] Offline dependency package list is missing." >&2
    exit 2
fi
if [[ ! -d /out ]]; then
    echo "[FAIL] Output mount /out is missing." >&2
    exit 2
fi

export DEBIAN_FRONTEND=noninteractive
apt-get update
packages="$(grep -Ev '^[[:space:]]*(#|$)' /tmp/offline-dependencies.txt | tr '\n' ' ')"
if [[ -z "$packages" ]]; then
    echo "[FAIL] Offline dependency package list is empty." >&2
    exit 2
fi

# Download the complete dependency closure in a clean Ubuntu 24.04 base image.
# --download-only leaves the container unmodified and puts all required .debs in apt's cache.
apt-get install --download-only --no-install-recommends -y $packages
shopt -s nullglob
archives=(/var/cache/apt/archives/*.deb)
if [[ ${#archives[@]} -eq 0 ]]; then
    echo "[FAIL] APT did not download any .deb packages." >&2
    exit 3
fi
cp -v "${archives[@]}" /out/
dpkg --print-architecture > /out/ARCHITECTURE
printf '%s\n' "$packages" > /out/REQUESTED-PACKAGES.txt

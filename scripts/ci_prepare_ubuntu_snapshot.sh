#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SNAPSHOT_FILE="$ROOT/packaging/ubuntu-snapshot-id.txt"
if [[ ! -f "$SNAPSHOT_FILE" ]]; then
    echo "[FAIL] Pinned Ubuntu snapshot ID is missing." >&2
    exit 2
fi
SNAPSHOT_ID="$(tr -d '[:space:]' < "$SNAPSHOT_FILE")"
if [[ ! "$SNAPSHOT_ID" =~ ^[0-9]{8}T[0-9]{6}Z$ ]]; then
    echo "[FAIL] Invalid Ubuntu snapshot ID: $SNAPSHOT_ID" >&2
    exit 2
fi
if [[ ! -r /etc/os-release ]]; then
    echo "[FAIL] Cannot identify the CI operating system." >&2
    exit 2
fi
# shellcheck disable=SC1091
source /etc/os-release
if [[ "$ID" != "ubuntu" ]]; then
    echo "[FAIL] Offline dependency resolution supports Ubuntu 22.04 and 24.04." >&2
    exit 2
fi
case "$VERSION_ID" in
    22.04) ubuntu_codename="jammy" ;;
    24.04) ubuntu_codename="noble" ;;
    *)
        echo "[FAIL] Offline dependency resolution supports Ubuntu 22.04 and 24.04; detected ${PRETTY_NAME:-unknown}." >&2
        exit 2
        ;;
esac

echo "[INFO] Configuring the pinned Ubuntu archive snapshot: $SNAPSHOT_ID"
# Keep this CI runner's package sources isolated to the official Ubuntu archive.
# No package installation or dpkg database changes are performed here.
for source_file in /etc/apt/sources.list /etc/apt/sources.list.d/*.sources /etc/apt/sources.list.d/*.list; do
    [[ -f "$source_file" ]] || continue
    [[ "$source_file" == /etc/apt/sources.list.d/cppapplab-snapshot.sources ]] && continue
    sudo mv "$source_file" "$source_file.cppapplab-disabled"
done

sudo tee /etc/apt/sources.list.d/cppapplab-snapshot.sources >/dev/null <<EOF
Types: deb
URIs: https://snapshot.ubuntu.com/ubuntu/$SNAPSHOT_ID
Suites: $ubuntu_codename $ubuntu_codename-updates $ubuntu_codename-security
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
Check-Valid-Until: no
EOF

sudo apt-get -o Acquire::Retries=3 -o Acquire::https::Timeout=30 update
echo "[PASS] Ubuntu snapshot indexes refreshed; no packages were installed."
echo "  Snapshot: $SNAPSHOT_ID"

#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUTPUT="$ROOT/dist"
if [[ $# -ge 1 && -n "$1" ]]; then OUTPUT="$1"; fi
if [[ "$OUTPUT" != /* ]]; then OUTPUT="$ROOT/$OUTPUT"; fi

if ! command -v docker >/dev/null 2>&1; then
    echo "[FAIL] Docker is required to resolve dependencies in a clean Ubuntu 24.04 image." >&2
    exit 2
fi
if ! git -C "$ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "[FAIL] Offline dependency packaging requires a Git checkout." >&2
    exit 2
fi

VERSION="$(awk '$1 == "VERSION" { print $2; exit }' "$ROOT/CMakeLists.txt")"
COMMIT="$(git -C "$ROOT" rev-parse HEAD)"
SHORT_SHA="$(printf '%s' "$COMMIT" | cut -c1-12)"
SOURCE_DATE_EPOCH="$(git -C "$ROOT" log -1 --format=%ct HEAD)"
PACKAGE_NAME="CppAppLab-offline-deps-ubuntu24.04-amd64-v$VERSION-$SHORT_SHA"

mkdir -p "$OUTPUT"
OUTPUT="$(cd "$OUTPUT" && pwd)"
TEMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TEMP_ROOT"' EXIT
BUNDLE_ROOT="$TEMP_ROOT/$PACKAGE_NAME"
mkdir -p "$BUNDLE_ROOT/packages"

echo "[INFO] Resolving the Ubuntu 24.04 amd64 dependency closure in Docker."
docker run --rm --platform linux/amd64 \
    -v "$ROOT/packaging/download-offline-deps.sh:/tmp/download-offline-deps.sh:ro" \
    -v "$ROOT/packaging/offline-dependencies.txt:/tmp/offline-dependencies.txt:ro" \
    -v "$BUNDLE_ROOT/packages:/out" \
    ubuntu:24.04 bash /tmp/download-offline-deps.sh

install -m 0644 "$ROOT/packaging/OFFLINE-DEPENDENCIES-README.md" "$BUNDLE_ROOT/README.md"
install -m 0755 "$ROOT/scripts/install_offline_dependencies.sh" "$BUNDLE_ROOT/install.sh"
cp "$BUNDLE_ROOT/packages/REQUESTED-PACKAGES.txt" "$BUNDLE_ROOT/PACKAGES.txt"
rm -f "$BUNDLE_ROOT/packages/REQUESTED-PACKAGES.txt" "$BUNDLE_ROOT/packages/ARCHITECTURE"
printf 'amd64\n' > "$BUNDLE_ROOT/ARCHITECTURE"

cat > "$BUNDLE_ROOT/BUILD-INFO.txt" <<EOF
Project: CppAppLab
Version: $VERSION
Source commit: $COMMIT
Target: Ubuntu 24.04 LTS amd64
Requested packages: build-essential cmake git gdb libgtest-dev clang-format clang-tidy
Dependency resolution: clean ubuntu:24.04 container
Install mode: local .deb files only; apt downloads disabled
EOF

(
    cd "$BUNDLE_ROOT"
    find packages -type f -name '*.deb' -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
    sha256sum README.md install.sh PACKAGES.txt ARCHITECTURE BUILD-INFO.txt >> SHA256SUMS
)

find "$BUNDLE_ROOT" -type f -exec touch -d "@$SOURCE_DATE_EPOCH" {} +
find "$BUNDLE_ROOT" -type d -exec touch -d "@$SOURCE_DATE_EPOCH" {} +

ARCHIVE="$OUTPUT/$PACKAGE_NAME.tar.gz"
tar --sort=name \
    --mtime="@$SOURCE_DATE_EPOCH" \
    --owner=0 --group=0 --numeric-owner \
    -C "$TEMP_ROOT" -cf - "$PACKAGE_NAME" | gzip -n > "$ARCHIVE"
sha256sum "$ARCHIVE" > "$ARCHIVE.sha256"

echo "[PASS] Offline dependency bundle created: $ARCHIVE"
echo "  .deb count: $(find "$BUNDLE_ROOT/packages" -maxdepth 1 -type f -name '*.deb' | wc -l)"
echo "  Size: $(stat -c '%s' "$ARCHIVE") bytes"

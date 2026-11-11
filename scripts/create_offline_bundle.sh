#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_PATH="${1:-$ROOT_DIR/dist/cppapplab-offline.tar.gz}"

for tool in cmake git tar sha256sum g++; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "[FAIL] Required bundle-generation tool is missing: $tool" >&2
        exit 1
    fi
done

if [[ -n "$(git -C "$ROOT_DIR" status --porcelain --untracked-files=all)" ]]; then
    echo "[FAIL] Commit or remove working-tree changes before generating a reproducible bundle." >&2
    exit 1
fi

mkdir -p "$(dirname "$OUTPUT_PATH")"
OUTPUT_DIR="$(cd "$(dirname "$OUTPUT_PATH")" && pwd)"
OUTPUT_PATH="$OUTPUT_DIR/$(basename "$OUTPUT_PATH")"

TEMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TEMP_DIR"' EXIT

FETCHCONTENT_DIR="$TEMP_DIR/fetchcontent"
PACKAGE_DIR="$TEMP_DIR/package"
mkdir -p "$FETCHCONTENT_DIR" "$PACKAGE_DIR"

echo "[INFO] Fetching the pinned GoogleTest v1.18.0 source once..."
cmake -S "$ROOT_DIR" -B "$TEMP_DIR/prefetch" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON \
    -DCPPAPPLAB_FETCH_GTEST=ON \
    -DCPPAPPLAB_GTEST_SOURCE_DIR= \
    -DCMAKE_DISABLE_FIND_PACKAGE_GTest=TRUE \
    -DFETCHCONTENT_BASE_DIR="$FETCHCONTENT_DIR"

GTEST_SOURCE_DIR="$FETCHCONTENT_DIR/googletest-src"
if [[ ! -f "$GTEST_SOURCE_DIR/CMakeLists.txt" ]]; then
    echo "[FAIL] GoogleTest source was not populated at $GTEST_SOURCE_DIR" >&2
    exit 1
fi

if ! git -C "$GTEST_SOURCE_DIR" describe --tags --exact-match HEAD 2>/dev/null | grep -qx 'v1.18.0'; then
    echo "[FAIL] Fetched GoogleTest source is not pinned to v1.18.0." >&2
    exit 1
fi

echo "[INFO] Staging the committed CppAppLab source tree..."
git -C "$ROOT_DIR" archive --format=tar HEAD | tar -xf - -C "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR/third_party"
cp -a "$GTEST_SOURCE_DIR" "$PACKAGE_DIR/third_party/googletest"
rm -rf "$PACKAGE_DIR/third_party/googletest/.git"

SOURCE_COMMIT="$(git -C "$ROOT_DIR" rev-parse HEAD)"
GENERATED_AT="$(date -u +'%Y-%m-%dT%H:%M:%SZ')"
cat > "$PACKAGE_DIR/OFFLINE_BUNDLE_MANIFEST.txt" <<EOF
CppAppLab offline source bundle
Generated UTC: $GENERATED_AT
CppAppLab commit: $SOURCE_COMMIT
GoogleTest source: v1.18.0
GoogleTest location: third_party/googletest

Included:
- Committed CppAppLab source, lessons, fixtures, scripts and documentation
- Pinned GoogleTest v1.18.0 source tree

Not included:
- Ubuntu/WSL2 operating system packages
- GCC, CMake, GDB, Git, clang-format or clang-tidy binaries

Install the target toolchain first. Then extract this bundle and run:
  bash scripts/check_environment.sh --strict-wsl
  bash scripts/build_offline.sh

The offline build forces CMake to use the bundled GoogleTest source and disables
CMake package discovery and FetchContent network access.
EOF

tar -czf "$OUTPUT_PATH" -C "$TEMP_DIR" package
sha256sum "$OUTPUT_PATH" > "$OUTPUT_PATH.sha256"

echo "[PASS] Offline source bundle created:"
echo "       $OUTPUT_PATH"
echo "[PASS] SHA-256:"
cat "$OUTPUT_PATH.sha256"

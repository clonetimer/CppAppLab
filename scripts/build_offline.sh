#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$ROOT_DIR/build-offline}"
GTEST_SOURCE_DIR="$ROOT_DIR/third_party/googletest"

if [[ ! -f "$GTEST_SOURCE_DIR/CMakeLists.txt" ]]; then
    echo "[FAIL] Bundled GoogleTest source is missing: $GTEST_SOURCE_DIR" >&2
    echo "       Extract a CppAppLab offline source bundle before running this script." >&2
    exit 1
fi

for tool in cmake ctest g++; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "[FAIL] Required offline build tool is missing: $tool" >&2
        exit 1
    fi
done

echo "[INFO] Configuring with the bundled GoogleTest source; network FetchContent is disabled."
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON \
    -DCPPAPPLAB_FETCH_GTEST=ON \
    -DCPPAPPLAB_GTEST_SOURCE_DIR="$GTEST_SOURCE_DIR" \
    -DCMAKE_DISABLE_FIND_PACKAGE_GTest=TRUE \
    -DFETCHCONTENT_FULLY_DISCONNECTED=ON

cmake --build "$BUILD_DIR" --parallel
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo "[PASS] CppAppLab built and tested using only the bundled GoogleTest source."

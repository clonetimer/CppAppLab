#!/usr/bin/env bash
set -Eeuo pipefail

if [[ $# -ne 1 || -z "$1" || ! -f "$1" ]]; then
    echo "Usage: $0 <learner-package.tar.gz>" >&2
    exit 64
fi
ARCHIVE="$1"

TEMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TEMP_ROOT"' EXIT

tar -xzf "$ARCHIVE" -C "$TEMP_ROOT"
PACKAGE_ROOT="$(find "$TEMP_ROOT" -mindepth 1 -maxdepth 1 -type d -name 'CppAppLab-learner-*' -print -quit)"
if [[ -z "$PACKAGE_ROOT" || ! -x "$PACKAGE_ROOT/cppapplab" ]]; then
    echo "[FAIL] Learner package layout or wrapper is invalid." >&2
    exit 1
fi

export HOME="$TEMP_ROOT/home"
export CPPAPPLAB_STATE_DIR="$TEMP_ROOT/state"
mkdir -p "$HOME"

VERSION_OUTPUT="$("$PACKAGE_ROOT/cppapplab" --version)"
LIST_OUTPUT="$("$PACKAGE_ROOT/cppapplab" list)"
grep -Fq "CppAppLab" <<< "$VERSION_OUTPUT"
grep -Fq "w1.hello-world" <<< "$LIST_OUTPUT"
grep -Fq "w8.http-operational-load" <<< "$LIST_OUTPUT"
grep -Fq "w9.static-root" <<< "$LIST_OUTPUT"

"$PACKAGE_ROOT/cppapplab" learn w1.hello-world > "$TEMP_ROOT/learn.log"
WORKSPACE="$HOME/cppapplab-work/w1.hello-world"
if [[ ! -f "$WORKSPACE/.cppapplab-workspace" || ! -f "$WORKSPACE/src/main.cpp" ]]; then
    echo "[FAIL] Relocated learner package did not create the expected workspace." >&2
    cat "$TEMP_ROOT/learn.log" >&2
    exit 1
fi

cat > "$WORKSPACE/src/main.cpp" <<'CPP'
#include <iostream>

int main() {
    std::cout << "Hello, CppAppLab!\n";
    return 0;
}
CPP

"$PACKAGE_ROOT/cppapplab" check "$WORKSPACE" > "$TEMP_ROOT/check.log"
grep -Fq "Result: PASS" "$TEMP_ROOT/check.log"

echo "[PASS] Relocated learner package smoke test."
echo "  CLI version: $VERSION_OUTPUT"
echo "  Catalog: W1, W8 and W9 tasks discovered."
echo "  Workspace: created from the packaged learner template."
echo "  Acceptance: W1 Hello World passed using the packaged runtime."

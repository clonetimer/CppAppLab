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
if [[ -z "$VERSION_OUTPUT" ]] || ! grep -Fq "Version: $VERSION_OUTPUT" "$PACKAGE_ROOT/BUILD-INFO.txt"; then
    echo "[FAIL] CLI version does not match the packaged build metadata." >&2
    echo "CLI version: $VERSION_OUTPUT" >&2
    cat "$PACKAGE_ROOT/BUILD-INFO.txt" >&2
    exit 1
fi
for task_id in w1.hello-world w8.http-operational-load w9.static-root; do
    if ! grep -Fq "$task_id" <<< "$LIST_OUTPUT"; then
        echo "[FAIL] Relocated package catalog is missing $task_id." >&2
        printf '%s\n' "$LIST_OUTPUT" >&2
        exit 1
    fi
done

if ! "$PACKAGE_ROOT/cppapplab" guide > "$TEMP_ROOT/guide.log"; then
    echo "[FAIL] Packaged Chinese learning guide did not open." >&2
    exit 1
fi
for task_id in w1.hello-world w8.http-operational-load w9.static-root; do
    if ! grep -Fq "$task_id" "$TEMP_ROOT/guide.log"; then
        echo "[FAIL] Packaged Chinese guide is missing $task_id." >&2
        exit 1
    fi
done

if ! "$PACKAGE_ROOT/cppapplab" learn w1.hello-world > "$TEMP_ROOT/learn.log"; then
    echo "[FAIL] The relocated package could not open the W1 learner task." >&2
    cat "$TEMP_ROOT/learn.log" >&2
    exit 1
fi
if ! grep -Fq "'$PACKAGE_ROOT/cppapplab' hint" "$TEMP_ROOT/learn.log" ||
   ! grep -Fq "'$PACKAGE_ROOT/cppapplab' check" "$TEMP_ROOT/learn.log" ||
   ! grep -Fq "'$PACKAGE_ROOT/cppapplab' learn" "$TEMP_ROOT/learn.log"; then
    echo "[FAIL] Packaged learner instructions must use this package's executable launcher." >&2
    cat "$TEMP_ROOT/learn.log" >&2
    exit 1
fi
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

if ! "$PACKAGE_ROOT/cppapplab" check "$WORKSPACE" > "$TEMP_ROOT/check.log"; then
    echo "[FAIL] Packaged W1 acceptance check returned a failure code." >&2
    cat "$TEMP_ROOT/check.log" >&2
    exit 1
fi
if ! grep -Fq "Result: PASS" "$TEMP_ROOT/check.log"; then
    echo "[FAIL] Packaged W1 acceptance output did not report PASS." >&2
    cat "$TEMP_ROOT/check.log" >&2
    exit 1
fi

echo "[PASS] Relocated learner package smoke test."
echo "  CLI version: $VERSION_OUTPUT"
echo "  Catalog: W1, W8 and W9 tasks discovered."
echo "  Workspace: created from the packaged learner template."
echo "  Acceptance: W1 Hello World passed using the packaged runtime."
echo "  Guide: Chinese 23-task route is accessible from the extracted package."
echo "  Commands: generated hints/check/continue commands use the packaged launcher."

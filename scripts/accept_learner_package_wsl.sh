#!/usr/bin/env bash
set -Eeuo pipefail

if [[ $# -ne 1 || ! -f "$1" ]]; then
    echo "Usage: $0 <CppAppLab-learner-ubuntu24.04-x86_64-*.tar.gz>" >&2
    exit 64
fi

ARCHIVE="$(realpath "$1")"
TEMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TEMP_ROOT"' EXIT

mkdir -p "$TEMP_ROOT/extract" "$TEMP_ROOT/home" "$TEMP_ROOT/state"
tar -xzf "$ARCHIVE" -C "$TEMP_ROOT/extract"
PACKAGE_ROOT="$(find "$TEMP_ROOT/extract" -mindepth 1 -maxdepth 1 -type d -name 'CppAppLab-learner-*' -print -quit)"
if [[ -z "$PACKAGE_ROOT" || ! -x "$PACKAGE_ROOT/cppapplab" ]]; then
    echo "[FAIL] Learner package layout or launcher is invalid." >&2
    exit 1
fi
if [[ ! -r /etc/os-release ]]; then
    echo "[FAIL] Cannot identify the target Ubuntu distribution." >&2
    exit 2
fi
# shellcheck disable=SC1091
source /etc/os-release
if [[ "$ID" != "ubuntu" || ( "$VERSION_ID" != "22.04" && "$VERSION_ID" != "24.04" ) ]]; then
    echo "[FAIL] Learner package acceptance supports Ubuntu 22.04 and 24.04; detected ${PRETTY_NAME:-unknown}." >&2
    exit 2
fi
PACKAGE_NAME="$(basename "$PACKAGE_ROOT")"
if [[ "$PACKAGE_NAME" != "CppAppLab-learner-ubuntu$VERSION_ID-x86_64-"* ]]; then
    echo "[FAIL] This learner package is not built for the current Ubuntu $VERSION_ID environment: $PACKAGE_NAME" >&2
    exit 2
fi

export HOME="$TEMP_ROOT/home"
export CPPAPPLAB_STATE_DIR="$TEMP_ROOT/state"

VERSION="$("$PACKAGE_ROOT/cppapplab" --version)"
if [[ -z "$VERSION" ]] || ! grep -Fq "Version: $VERSION" "$PACKAGE_ROOT/BUILD-INFO.txt"; then
    echo "[FAIL] CLI version and package build metadata do not match." >&2
    exit 1
fi

LIST_OUTPUT="$("$PACKAGE_ROOT/cppapplab" list)"
for task_id in w1.hello-world w8.http-operational-load w9.static-root; do
    if ! grep -Fq "$task_id" <<< "$LIST_OUTPUT"; then
        echo "[FAIL] Packaged catalog is missing $task_id." >&2
        exit 1
    fi
done

"$PACKAGE_ROOT/cppapplab" learn w1.hello-world > "$TEMP_ROOT/learn.log"
WORKSPACE="$HOME/cppapplab-work/w1.hello-world"
if [[ ! -f "$WORKSPACE/.cppapplab-workspace" || ! -f "$WORKSPACE/src/main.cpp" ]]; then
    echo "[FAIL] The packaged CLI did not create the W1 workspace." >&2
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
if ! grep -Fq "Result: PASS" "$TEMP_ROOT/check.log"; then
    echo "[FAIL] W1 acceptance did not pass." >&2
    cat "$TEMP_ROOT/check.log" >&2
    exit 1
fi

"$PACKAGE_ROOT/cppapplab" progress > "$TEMP_ROOT/progress-before.log"
"$PACKAGE_ROOT/cppapplab" history w1.hello-world > "$TEMP_ROOT/history-before.log"
if ! grep -Fq "PASSED" "$TEMP_ROOT/progress-before.log" ||
   ! grep -Fq "PASS" "$TEMP_ROOT/history-before.log"; then
    echo "[FAIL] The package did not persist W1 PASS evidence." >&2
    cat "$TEMP_ROOT/progress-before.log" >&2
    cat "$TEMP_ROOT/history-before.log" >&2
    exit 1
fi

RELOCATED_PACKAGE="$TEMP_ROOT/relocated learner package"
mv "$PACKAGE_ROOT" "$RELOCATED_PACKAGE"

RELOCATED_VERSION="$("$RELOCATED_PACKAGE/cppapplab" --version)"
"$RELOCATED_PACKAGE/cppapplab" progress > "$TEMP_ROOT/progress-after.log"
"$RELOCATED_PACKAGE/cppapplab" history w1.hello-world > "$TEMP_ROOT/history-after.log"
"$RELOCATED_PACKAGE/cppapplab" status "$WORKSPACE" > "$TEMP_ROOT/status-after.log"

if [[ "$RELOCATED_VERSION" != "$VERSION" ]] ||
   ! grep -Fq "PASSED" "$TEMP_ROOT/progress-after.log" ||
   ! grep -Fq "PASS" "$TEMP_ROOT/history-after.log" ||
   ! grep -Fq "Last recorded result: PASS" "$TEMP_ROOT/status-after.log"; then
    echo "[FAIL] Package relocation changed the CLI or lost learner progress." >&2
    cat "$TEMP_ROOT/progress-after.log" >&2
    cat "$TEMP_ROOT/history-after.log" >&2
    cat "$TEMP_ROOT/status-after.log" >&2
    exit 1
fi

echo "[PASS] Existing Ubuntu $VERSION_ID WSL learner-package acceptance."
echo "  CLI: $VERSION"
echo "  Catalog: W1, W8 and W9 tasks discovered."
echo "  W1: learner workspace created and fixed acceptance passed."
echo "  Persistence: progress and check history survived package relocation."
echo "  Relocated package path: $RELOCATED_PACKAGE"
echo "  No Ubuntu installation or package installation was performed."

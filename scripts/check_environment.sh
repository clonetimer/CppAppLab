#!/usr/bin/env bash
set -Eeuo pipefail

STRICT_WSL=0
if [[ "${1:-}" == "--strict-wsl" ]]; then
    STRICT_WSL=1
elif [[ $# -gt 0 ]]; then
    echo "Usage: $0 [--strict-wsl]" >&2
    exit 2
fi

failures=0
warnings=0

pass() { printf '[PASS] %s\n' "$1"; }
warn() { printf '[WARN] %s\n' "$1"; warnings=$((warnings + 1)); }
fail() { printf '[FAIL] %s\n' "$1"; failures=$((failures + 1)); }

if [[ -r /etc/os-release ]]; then
    # shellcheck disable=SC1091
    source /etc/os-release
    if [[ "${ID:-}" == "ubuntu" && "${VERSION_ID:-}" == "24.04" ]]; then
        pass "Ubuntu 24.04 detected"
    else
        fail "Expected Ubuntu 24.04; detected ${PRETTY_NAME:-unknown}"
    fi
else
    fail "Cannot read /etc/os-release"
fi

kernel_release="$(uname -r)"
if [[ "$kernel_release" == *microsoft-standard-WSL2* ]]; then
    pass "WSL2 kernel detected"
elif [[ "$kernel_release" == *microsoft* ]]; then
    if [[ "$STRICT_WSL" -eq 1 ]]; then
        fail "A Microsoft WSL kernel was detected, but it does not identify itself as WSL2"
    else
        warn "Microsoft WSL kernel detected; verify VERSION 2 with 'wsl -l -v' in PowerShell"
    fi
else
    if [[ "$STRICT_WSL" -eq 1 ]]; then
        fail "WSL2 was not detected"
    else
        warn "WSL2 was not detected; use 'wsl -l -v' in PowerShell to verify the target environment"
    fi
fi

for tool in g++ cmake git gdb; do
    if command -v "$tool" >/dev/null 2>&1; then
        pass "$tool: $(command -v "$tool")"
    else
        fail "$tool is not installed or not on PATH"
    fi
done

for tool in clang-format clang-tidy; do
    if command -v "$tool" >/dev/null 2>&1; then
        pass "$tool: $(command -v "$tool")"
    else
        warn "$tool is optional for the initial build but required by the full quality workflow"
    fi
done

if command -v cmake >/dev/null 2>&1; then
    cmake_version="$(cmake --version | awk 'NR == 1 {print $3}')"
    if dpkg --compare-versions "$cmake_version" ge "3.18"; then
        pass "CMake $cmake_version meets the 3.18 minimum"
    else
        fail "CMake $cmake_version is older than 3.18"
    fi
fi

if command -v g++ >/dev/null 2>&1 && command -v gdb >/dev/null 2>&1; then
    temp_dir="$(mktemp -d)"
    trap 'rm -rf "$temp_dir"' EXIT

    cat > "$temp_dir/hello.cpp" <<'CPP'
#include <iostream>

int main() {
    std::cout << "CppAppLab toolchain check: C++17 OK\n";
    return 0;
}
CPP

    if g++ -std=c++17 -Wall -Wextra -Wpedantic -g \
        "$temp_dir/hello.cpp" -o "$temp_dir/hello"; then
        pass "GCC compiled a C++17 smoke program"
    else
        fail "GCC could not compile the C++17 smoke program"
    fi

    if [[ -x "$temp_dir/hello" ]] && "$temp_dir/hello"; then
        pass "Compiled smoke program executed"
    else
        fail "Compiled smoke program did not execute successfully"
    fi

    if [[ -x "$temp_dir/hello" ]] &&
        gdb --batch --quiet -ex "break main" -ex "run" -ex "continue" \
            --args "$temp_dir/hello" >/dev/null 2>&1; then
        pass "GDB ran the smoke program and hit main"
    else
        fail "GDB smoke check failed"
    fi
fi

printf '\nSummary: %s failure(s), %s warning(s)\n' "$failures" "$warnings"
if [[ "$failures" -gt 0 ]]; then
    exit 1
fi

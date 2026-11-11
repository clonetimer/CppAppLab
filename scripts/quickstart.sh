#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [[ ! -r /etc/os-release ]]; then
    echo "[FAIL] Cannot identify the Linux distribution." >&2
    exit 1
fi
# shellcheck disable=SC1091
source /etc/os-release
if [[ "${ID:-}" != "ubuntu" || "${VERSION_ID:-}" != "24.04" ]]; then
    echo "[FAIL] CppAppLab v1 targets Ubuntu 24.04; detected ${PRETTY_NAME:-unknown}." >&2
    echo "See docs/ENVIRONMENT_WSL2_UBUNTU.md for the supported environment." >&2
    exit 1
fi

required_packages=(build-essential cmake git gdb libgtest-dev)
missing_packages=()
for package in "${required_packages[@]}"; do
    if ! dpkg-query -W -f='${Status}' "$package" 2>/dev/null | grep -q 'install ok installed'; then
        missing_packages+=("$package")
    fi
done

if (( ${#missing_packages[@]} > 0 )); then
    printf '[INFO] Missing packages: %s\n' "${missing_packages[*]}"
    printf 'Install command: sudo apt-get update && sudo apt-get install -y %s\n' "${missing_packages[*]}"
    if [[ ! -t 0 ]]; then
        echo "[FAIL] No interactive terminal is available to approve package installation." >&2
        exit 1
    fi

    read -r -p "Install these packages now? [Y/n] " answer
    case "${answer,,}" in
        n|no)
            echo "Setup stopped. Install the packages above, then rerun this script."
            exit 1
            ;;
        *)
            sudo apt-get update
            sudo apt-get install -y "${missing_packages[@]}"
            ;;
    esac
fi

echo
echo "== Environment check =="
bash scripts/check_environment.sh

echo
echo "== Configure =="
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCPPAPPLAB_FETCH_GTEST=OFF

echo
echo "== Build =="
cmake --build build --parallel

echo
echo "== Platform tests =="
ctest --test-dir build --output-on-failure

echo
echo "== Open interactive learning home =="
exec ./build/cppapplab

#!/usr/bin/env bash
set -Eeuo pipefail

mode="${1:-cli}"
if [[ "$mode" != "cli" && "$mode" != "--web" ]]; then
    echo "Usage: bash scripts/quickstart.sh [--web]" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# shellcheck source=scripts/lib/ubuntu-support.sh
source "$ROOT/scripts/lib/ubuntu-support.sh"
if ! cppapplab_check_ubuntu; then
    echo "See docs/ENVIRONMENT_WSL2_UBUNTU.md for the supported environment." >&2
    exit 1
fi

# shellcheck source=scripts/lib/terminal.sh
source "$ROOT/scripts/lib/terminal.sh"

required_packages=(build-essential cmake git gdb libgtest-dev libssl-dev openssl)
missing_packages=()
for package in "${required_packages[@]}"; do
    if ! dpkg-query -W -f='${Status}' "$package" 2>/dev/null | grep -q 'install ok installed'; then
        missing_packages+=("$package")
    fi
done

if (( ${#missing_packages[@]} > 0 )); then
    printf '[INFO] Missing packages: %s\n' "${missing_packages[*]}"
    printf 'Install command: sudo apt-get update && sudo apt-get install -y %s\n' "${missing_packages[*]}"

    if cppapplab_prompt_for_confirmation "Install these packages now? [Y/n] "; then
        if [[ "$EUID" -eq 0 ]]; then
            apt_command=(apt-get)
        else
            if ! command -v sudo >/dev/null 2>&1; then
                echo "[FAIL] sudo is required to install the missing Ubuntu packages." >&2
                exit 1
            fi
            # Obtain credentials while the controlling terminal is available.
            sudo -v
            apt_command=(sudo apt-get)
        fi
        "${apt_command[@]}" update
        "${apt_command[@]}" install -y "${missing_packages[@]}"
    else
        prompt_status=$?
        if [[ "$prompt_status" -eq 1 ]]; then
            echo "Setup stopped. Install the packages above, then rerun this script."
        fi
        exit 1
    fi
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
if [[ "$mode" == "--web" ]]; then
    echo "== Start browser learning workbench =="
    CPPAPPLAB_OPEN_BROWSER=1 exec bash scripts/start-web-workbench.sh
fi

echo "== Open interactive learning home =="
exec ./build/cppapplab

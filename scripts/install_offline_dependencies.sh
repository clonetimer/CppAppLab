#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
WITH_QUALITY=0
DRY_RUN=0
VERIFY_ONLY=0

for arg in "$@"; do
    case "$arg" in
        --with-quality-tools) WITH_QUALITY=1 ;;
        --dry-run) DRY_RUN=1 ;;
        --verify-only) VERIFY_ONLY=1 ;;
        -h|--help)
            cat <<'HELP'
Usage: ./install.sh [--with-quality-tools] [--dry-run | --verify-only]

Default: install the required C++ learner/build toolchain from local .deb files.
--with-quality-tools: also install clang-format and clang-tidy.
--dry-run: ask APT to simulate installation; no packages are changed or downloaded.
--verify-only: verify bundle checksums and Debian package metadata without APT installation.
HELP
            exit 0
            ;;
        *)
            echo "[FAIL] Unknown option: $arg" >&2
            exit 64
            ;;
    esac
done
if [[ "$DRY_RUN" -eq 1 && "$VERIFY_ONLY" -eq 1 ]]; then
    echo "[FAIL] Choose either --dry-run or --verify-only." >&2
    exit 64
fi

if [[ ! -r /etc/os-release ]]; then
    echo "[FAIL] Cannot identify the operating system." >&2
    exit 2
fi
# shellcheck disable=SC1091
source /etc/os-release
if [[ "$ID" != "ubuntu" || "$VERSION_ID" != "24.04" ]]; then
    echo "[FAIL] This dependency bundle targets an existing Ubuntu 24.04 installation; detected ${PRETTY_NAME:-unknown}." >&2
    exit 2
fi
if [[ "$(dpkg --print-architecture)" != "amd64" ]]; then
    echo "[FAIL] This dependency bundle targets amd64." >&2
    exit 2
fi

(
    cd "$ROOT"
    sha256sum --check SHA256SUMS
)

shopt -s nullglob
base_debs=("$ROOT"/packages/base/*.deb)
quality_debs=("$ROOT"/packages/quality/*.deb)
if [[ "${#base_debs[@]}" -eq 0 ]]; then
    echo "[FAIL] Required local .deb packages are missing." >&2
    exit 2
fi
selected_debs=("${base_debs[@]}")
if [[ "$WITH_QUALITY" -eq 1 ]]; then
    if [[ "${#quality_debs[@]}" -eq 0 ]]; then
        echo "[FAIL] Optional quality .deb packages are missing." >&2
        exit 2
    fi
    selected_debs+=("${quality_debs[@]}")
fi

declare -A package_names=()
for deb in "${selected_debs[@]}"; do
    name="$(dpkg-deb -f "$deb" Package)"
    version="$(dpkg-deb -f "$deb" Version)"
    architecture="$(dpkg-deb -f "$deb" Architecture)"
    if [[ "$architecture" != "amd64" && "$architecture" != "all" ]]; then
        echo "[FAIL] Unsupported package architecture $architecture: $deb" >&2
        exit 3
    fi
    identity="$name=$version"
    if [[ -n "${package_names[$identity]:-}" ]]; then
        echo "[FAIL] Duplicate Debian package identity: $identity" >&2
        exit 3
    fi
    package_names["$identity"]=1
done

for required in build-essential cmake git gdb libgtest-dev; do
    found=0
    for deb in "${base_debs[@]}"; do
        [[ "$(dpkg-deb -f "$deb" Package)" == "$required" ]] && found=1
    done
    if [[ "$found" -ne 1 ]]; then
        echo "[FAIL] Required package is absent from the base bundle: $required" >&2
        exit 3
    fi
done
if [[ "$WITH_QUALITY" -eq 1 ]]; then
    for optional in clang-format clang-tidy; do
        found=0
        for deb in "${quality_debs[@]}"; do
            [[ "$(dpkg-deb -f "$deb" Package)" == "$optional" ]] && found=1
        done
        if [[ "$found" -ne 1 ]]; then
            echo "[FAIL] Optional quality package is absent: $optional" >&2
            exit 3
        fi
    done
fi

echo "[PASS] Bundle checksums, architecture and package metadata verified."
if [[ "$VERIFY_ONLY" -eq 1 ]]; then
    echo "[PASS] Verify-only mode: no package installation or APT operation performed."
    exit 0
fi

if [[ "$DRY_RUN" -eq 1 ]]; then
    echo "[INFO] Simulating a clean-target installation using only a temporary local APT repository."
    TEMP_ROOT="$(mktemp -d)"
    trap 'rm -rf "$TEMP_ROOT"' EXIT
    LOCAL_REPO="$TEMP_ROOT/repo"
    mkdir -p "$LOCAL_REPO" "$TEMP_ROOT/lists/partial" "$TEMP_ROOT/archives/partial"
    for deb in "${selected_debs[@]}"; do
        ln -s "$deb" "$LOCAL_REPO/$(basename "$deb")"
    done
    python3 "$ROOT/scripts/build_local_apt_index.py" --repo "$LOCAL_REPO"
    printf 'deb [trusted=yes] file:%s ./\n' "$LOCAL_REPO" > "$TEMP_ROOT/sources.list"
    EMPTY_STATUS="$TEMP_ROOT/status"
    : > "$EMPTY_STATUS"
    APT_OPTIONS=(
        -o "Dir::State::status=$EMPTY_STATUS"
        -o "Dir::State::lists=$TEMP_ROOT/lists"
        -o "Dir::Cache::archives=$TEMP_ROOT/archives"
        -o "Dir::Etc::sourcelist=$TEMP_ROOT/sources.list"
        -o "Dir::Etc::sourceparts=-"
        -o "APT::Get::List-Cleanup=0"
        -o Debug::NoLocking=1
    )
    apt-get "${APT_OPTIONS[@]}" update
    if [[ "$WITH_QUALITY" -eq 1 ]]; then
        roots=(build-essential cmake git gdb libgtest-dev clang-format clang-tidy)
    else
        roots=(build-essential cmake git gdb libgtest-dev)
    fi
    apt-get "${APT_OPTIONS[@]}" --simulate --no-install-recommends install "${roots[@]}"
    echo "[PASS] Clean-target local-repository APT simulation completed; no packages were installed."
    exit 0
fi

echo "[INFO] Installing only from bundled .deb files. Network downloads are disabled."
if [[ "$EUID" -eq 0 ]]; then
    apt-get --no-download --no-install-recommends install -y "${selected_debs[@]}"
else
    sudo apt-get --no-download --no-install-recommends install -y "${selected_debs[@]}"
fi

for tool in g++ cmake git gdb; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "[FAIL] Installed bundle did not provide $tool." >&2
        exit 4
    fi
done
if [[ "$WITH_QUALITY" -eq 1 ]]; then
    for tool in clang-format clang-tidy; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            echo "[FAIL] Optional quality installation did not provide $tool." >&2
            exit 4
        fi
    done
fi
if ! dpkg-query -W -f='${Status}' libgtest-dev 2>/dev/null | grep -q 'install ok installed'; then
    echo "[FAIL] libgtest-dev is not installed." >&2
    exit 4
fi
echo "[PASS] CppAppLab offline dependencies installed."

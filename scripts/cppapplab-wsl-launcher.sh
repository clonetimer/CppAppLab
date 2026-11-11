#!/usr/bin/env bash
set -Eeuo pipefail

normalize_repo_path() {
    local path="$1"
    case "$path" in
        "~/"*) printf '%s/%s\n' "$HOME" "${path#\~/}" ;;
        "~") printf '%s\n' "$HOME" ;;
        /*) printf '%s\n' "$path" ;;
        *)
            echo "[FAIL] RepoPath must be an absolute Linux path or begin with ~/." >&2
            return 2
            ;;
    esac
}

# This mode is used by CI to test the same path normalization used at launch.
if [[ "${1:-}" == "--test-normalize-path" ]]; then
    if [[ "$#" -ne 2 ]]; then
        echo "Usage: $0 --test-normalize-path <linux-path>" >&2
        exit 64
    fi
    normalize_repo_path "$2"
    exit $?
fi

repo_path_base64="${1:-}"
force_setup="${2:-0}"
if [[ -z "$repo_path_base64" ]]; then
    echo "[FAIL] Missing encoded Linux checkout path." >&2
    exit 64
fi
if [[ "$force_setup" != "0" && "$force_setup" != "1" ]]; then
    echo "[FAIL] Invalid setup mode; expected 0 or 1." >&2
    exit 64
fi
if [[ -z "${HOME:-}" ]]; then
    echo "[FAIL] WSL did not provide a HOME directory." >&2
    exit 2
fi

repo_path_raw="$(printf '%s' "$repo_path_base64" | base64 --decode)"
repo_path="$(normalize_repo_path "$repo_path_raw")"

if [[ ! -r /etc/os-release ]]; then
    echo "[FAIL] Cannot identify the Ubuntu distribution." >&2
    exit 2
fi
# shellcheck disable=SC1091
source /etc/os-release
if [[ "${ID:-}" != "ubuntu" || "${VERSION_ID:-}" != "24.04" ]]; then
    echo "[FAIL] CppAppLab requires Ubuntu 24.04; detected ${PRETTY_NAME:-unknown}." >&2
    exit 2
fi

if ! command -v git >/dev/null 2>&1; then
    echo "[INFO] Git is missing; installing it inside Ubuntu."
    sudo apt-get update
    sudo apt-get install -y git
fi

if [[ -e "$repo_path" ]] && ! git -C "$repo_path" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "[FAIL] The target path exists but is not a Git checkout: $repo_path" >&2
    echo "No existing files were changed. Choose another path with -RepoPath." >&2
    exit 3
fi

if ! git -C "$repo_path" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    mkdir -p "$(dirname "$repo_path")"
    echo "[INFO] Cloning the private CppAppLab repository into $repo_path"
    if ! git clone https://github.com/clonetimer/CppAppLab.git "$repo_path"; then
        echo "[FAIL] Git clone failed. Configure GitHub authentication for Git inside WSL (for example, install GitHub CLI and run 'gh auth login') and retry." >&2
        exit 5
    fi
fi

remote_url="$(git -C "$repo_path" remote get-url origin 2>/dev/null | tr '[:upper:]' '[:lower:]' || true)"
case "$remote_url" in
    https://github.com/clonetimer/cppapplab.git|https://github.com/clonetimer/cppapplab|git@github.com:clonetimer/cppapplab.git|git@github.com:clonetimer/cppapplab|ssh://git@github.com/clonetimer/cppapplab.git)
        ;;
    *)
        echo "[FAIL] The checkout at $repo_path is not the clonetimer/CppAppLab repository." >&2
        echo "No files were changed. Set -RepoPath to the correct Linux checkout." >&2
        exit 6
        ;;
esac

cd "$repo_path"
if [[ "$force_setup" == "1" || ! -x build/cppapplab ]]; then
    echo "[INFO] Running first-time setup/build/test."
    exec bash scripts/quickstart.sh
fi

echo "[INFO] Opening the existing CppAppLab build."
echo "[INFO] No automatic git pull is performed; local changes are preserved."
exec ./build/cppapplab

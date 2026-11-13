#!/usr/bin/env bash
# Shared operating-system check for CppAppLab source builds.
# The prebuilt learner and offline dependency packages have their own target
# contracts and are intentionally validated separately.

cppapplab_check_ubuntu() {
    local release_file="${1:-/etc/os-release}"
    if [[ ! -r "$release_file" ]]; then
        printf '[FAIL] Cannot read operating-system metadata: %s\n' "$release_file" >&2
        return 1
    fi

    local ID VERSION_ID PRETTY_NAME
    # shellcheck disable=SC1090
    source "$release_file"

    if [[ "$ID" == "ubuntu" ]]; then
        case "$VERSION_ID" in
            22.04|24.04)
                printf '[PASS] %s detected; supported CppAppLab source-build target.\n' "${PRETTY_NAME:-Ubuntu $VERSION_ID}"
                return 0
                ;;
        esac
    fi

    printf '[FAIL] CppAppLab source builds support Ubuntu 22.04 LTS and Ubuntu 24.04 LTS; detected %s.\n' "${PRETTY_NAME:-${ID:-unknown} ${VERSION_ID:-unknown}}" >&2
    return 1
}

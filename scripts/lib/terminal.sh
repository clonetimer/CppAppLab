#!/usr/bin/env bash

# Ask a question without assuming that stdin is connected to the terminal.
# The Windows/WSL bootstrap may read its own payload before handing off to
# quickstart.sh; in that case stdin can be at EOF while /dev/tty is still live.
# Return 0 for yes, 1 for an explicit no, and 2 when no usable terminal exists.
cppapplab_prompt_for_confirmation() {
    local prompt="$1"
    local answer=""

    if [[ -t 0 ]]; then
        if ! IFS= read -r -p "$prompt" answer; then
            printf '\\n[FAIL] Could not read the confirmation response from stdin.\\n' >&2
            return 2
        fi
    elif ( : </dev/tty ) 2>/dev/null; then
        if ! IFS= read -r -p "$prompt" answer </dev/tty; then
            printf '\\n[FAIL] Could not read the confirmation response from /dev/tty.\\n' >&2
            return 2
        fi
    else
        printf '[FAIL] No interactive terminal is available to approve package installation.\\n' >&2
        return 2
    fi

    case "${answer,,}" in
        n|no) return 1 ;;
        *) return 0 ;;
    esac
}

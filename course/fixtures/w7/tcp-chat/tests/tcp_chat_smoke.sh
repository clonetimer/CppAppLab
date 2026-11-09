#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 1 ]]; then
    echo "usage: tcp_chat_smoke.sh <tcp-chat-cli>" >&2
    exit 2
fi
output="$("$1" --help)"
grep -Fq "server <port>" <<<"$output"
echo "W7 TCP chat CLI smoke passed"

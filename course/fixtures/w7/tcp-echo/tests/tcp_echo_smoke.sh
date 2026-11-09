#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 1 ]]; then
    echo "usage: tcp_echo_smoke.sh <tcp-echo-cli>" >&2
    exit 2
fi
output="$("$1" --help)"
grep -Fq "server <port>" <<<"$output"
grep -Fq "client <port> <message>" <<<"$output"
echo "W7 TCP echo CLI smoke passed"

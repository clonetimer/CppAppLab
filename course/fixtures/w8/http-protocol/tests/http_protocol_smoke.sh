#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 1 ]]; then
    echo "usage: http_protocol_smoke.sh <http-protocol-cli>" >&2
    exit 2
fi
output="$("$1" --help)"
grep -Fq "parse-line" <<<"$output"
grep -Fq "response" <<<"$output"
echo "W8 HTTP protocol CLI smoke passed"

#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 1 ]]; then
    echo "usage: http_parser_smoke.sh <http-parser-cli>" >&2
    exit 2
fi
output="$("$1" --help)"
grep -Fq "parse-stdin" <<<"$output"
echo "W8 incremental HTTP parser CLI smoke passed"

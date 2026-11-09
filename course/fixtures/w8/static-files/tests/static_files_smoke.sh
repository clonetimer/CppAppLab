#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 1 ]]; then
    echo "usage: static_files_smoke.sh <http-static-files-cli>" >&2
    exit 2
fi
output="$("$1" --help)"
grep -Fq "handle" <<<"$output"
echo "W8 static-file CLI smoke passed"

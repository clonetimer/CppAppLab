#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: system_lab_cli_smoke.sh <system-lab>" >&2
  exit 2
fi

output="$("$1" /bin/echo "W5 process smoke")"
[[ "$output" == "W5 process smoke" ]]

set +e
"$1" /bin/sh -c 'exit 6'
status=$?
set -e
[[ "$status" -eq 6 ]]

echo "W5 CLI smoke passed"

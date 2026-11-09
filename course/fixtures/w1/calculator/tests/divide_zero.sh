#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: divide_zero.sh <calculator-executable>" >&2
    exit 2
fi

set +e
output="$("$1" divide 1 0 2>&1)"
status=$?
set -e

if [[ "$status" -ne 3 ]]; then
    echo "expected exit code 3, received $status" >&2
    echo "$output" >&2
    exit 1
fi

if [[ "$output" != "error: division by zero" ]]; then
    echo "unexpected division-by-zero diagnostic: $output" >&2
    exit 1
fi

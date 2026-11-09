#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 1 ]]; then echo "usage: thread_pool_smoke.sh <thread-pool-cli>" >&2; exit 2; fi
output="$("$1")"
grep -Fxq "CppAppLab W6" <<<"$output"
grep -Fxq "42" <<<"$output"
echo "W6 thread pool CLI smoke passed"

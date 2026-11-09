#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: task_manager_cli_workflow.sh <todo-executable>" >&2
    exit 2
fi

todo_executable="$1"
temporary_directory="$(mktemp -d)"
trap 'rm -rf "$temporary_directory"' EXIT
export CPPAPPLAB_TODO_FILE="$temporary_directory/tasks.tsv"

initial="$("$todo_executable" list)"
[[ "$initial" == "No tasks." ]]

"$todo_executable" add "Review class ownership"
"$todo_executable" add "Verify copy and move"

listed="$("$todo_executable" list)"
grep -Fxq '#1 [ ] Review class ownership' <<<"$listed"
grep -Fxq '#2 [ ] Verify copy and move' <<<"$listed"

"$todo_executable" done 1
listed="$("$todo_executable" list)"
grep -Fxq '#1 [x] Review class ownership' <<<"$listed"

"$todo_executable" remove 2
listed="$("$todo_executable" list)"
grep -Fxq '#1 [x] Review class ownership' <<<"$listed"
if grep -Fq '#2 ' <<<"$listed"; then
    echo "removed task is still present" >&2
    exit 1
fi

set +e
"$todo_executable" done 999 >"$temporary_directory/stdout.txt" 2>"$temporary_directory/stderr.txt"
missing_status=$?
set -e
[[ "$missing_status" -eq 3 ]]
grep -Fxq 'error: task not found' "$temporary_directory/stderr.txt"

echo "TaskManager CLI workflow passed"

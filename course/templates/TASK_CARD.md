# TASK-ID — Task title

## Prerequisites

- List required earlier tasks.

## Learning outcomes

- State what the learner should be able to explain or implement.

## Problem

Describe the behavior to implement without giving away the full solution.

## Learner-editable files

- \`path/to/file.cpp\`

## Stable interfaces

- List required public types, functions, and signatures.

## Constraints

- State required techniques and prohibited shortcuts.

## Build and check

\`\`\`bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
\`\`\`

## Acceptance cases

| Case | Input / setup | Expected behavior |
|---|---|---|
| Normal | TODO | TODO |
| Boundary | TODO | TODO |
| Error | TODO | TODO |

## Progressive hints

1. Concept hint
2. Design hint
3. Implementation hint

## Explain your design

- Why is this interface/ownership/error-handling strategy appropriate?
- Which boundary or failure case is the easiest to overlook?

## Expected artifact

Describe the executable, library, test, report, or other result to deliver.

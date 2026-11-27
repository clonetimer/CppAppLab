# W9 · A5 Parser Invariants and Bounded Fuzzing

## Start here

1. Read the fixed A4 admission interface and `include/admission_invariants.hpp`.
2. Edit only `src/admission_invariants.cpp`. Implement the observation validator and equality comparison, not another HTTP parser.
3. Begin with a correct accepted observation and a deliberately inconsistent observation constructed by the tests.
4. Build, run tests and formally check the learner implementation. Then optionally run the real Clang/libFuzzer profile on your own saved source.
5. Interpret the actual run count, coverage counters and any saved reproducer. Do not claim that no discovered failure proves security.

## Goal

Learn property-based verification of a local parser, with independent checks of observable outcomes, deterministic corpus replay, and a bounded ASan/UBSan-backed coverage-guided fuzz campaign. Reuse the fixed A1/A4 parser. All inputs are local bytes in memory; the target has no network client, remote target, credentials or external service testing.

## Your task

Implement `validate_observation` and `same_observation` in `src/admission_invariants.cpp`. The interface is read-only. The fixture deliberately supplies some inconsistent observations to ensure the learner oracle does not always return success.

| Priority | Observation contract |
|---|---|
| 1 | An unknown status returns UnknownStatus. |
| 2 | Incomplete/rejected observations must consume zero bytes and expose no request fields, else HiddenState; mandatory close matches rejection, else ClosePolicy. |
| 3 | Accepted observations must not require a policy close. |
| 4 | Accepted consumed length is positive, no greater than input and within wire budget, else ConsumedBoundary. |
| 5 | Method and target are nonempty, version is HTTP/1.0 or HTTP/1.1, HTTP/1.1 has nonempty Host, else RequestShape. |
| 6 | A complete header terminator occurs within the consumed prefix and header budget, else HeaderBoundary. |
| 7 | Header count, decoded body and body versus available wire-body size satisfy the A4 budgets, else ResourceBudget. |
| 8 | Otherwise return None. |

Compare status, consumed, mandatory close and every request field; unordered header insertion order has no semantic meaning. The platform-owned driver runs A4 on the same input twice and, for a complete accepted request, on its exact consumed prefix and one-byte-shorter prefix. It does not require you to write another grammar. These are selected invariants, not a proof of full parser correctness.

## Build and run

Use the usual Build, Run / Test and Formal Acceptance actions for 21 fixed GoogleTest cases. GCC C++17 remains the normal toolchain on both supported Ubuntu releases.

Optional bounded fuzz campaign on Ubuntu 24.04 (separate from formal course checks):

```bash
sudo apt-get install clang libclang-rt-dev
python3 scripts/verify_w9_fuzz.py --learner-root "$HOME/cppapplab-work/w9.http-fuzzing" --output artifacts/w9-a5-own-run
```

Run from a source checkout; the output directory must be new. Use your trusted local learner source only. The script stages a snapshot, compiles the actual fixed target with Clang/libFuzzer/ASan/UBSan, materializes the checked-in seed corpus, and runs a fixed 10,000-execution, 4 KiB input-length campaign. Results record toolchain, commands, source digests, seed digests, log, exit status and actual observed run/coverage counters. A failure preserves the actual local reproducer if the engine creates one; no reproduction is invented. Formal acceptance does not silently run the optional fuzz campaign.

## Explain your design

Why does an oracle that always returns None make a fuzz run misleading? Why is a synthetic inconsistent result useful for verifying the oracle itself? Which invariant checks distinguish a complete message from a partial one? Why does 10,000 executions without a finding not prove the absence of bugs?

## Troubleshooting

- Tests fail on deliberately inconsistent data: inspect the expected violation priority and do not just return None.
- Replay mismatches even with identical headers: compare map content, not iteration order.
- The optional compiler or runtime is unavailable: install the separate toolchain, keep the result unverified, do not substitute ordinary unit tests for fuzz execution.
- A campaign produces a crash artifact: preserve the exact artifact and log, reproduce locally under the same configuration, isolate the smallest triggering input before changing the oracle or parser. Do not test it on external services.

## Acceptance

- [ ] All 21 fixed tests pass with actual C++17 compilation.
- [ ] The starter fails the oracle mutation checks and a correct implementation passes.
- [ ] Replay handles normal, partial, binary, chunked and resource-boundary inputs.
- [ ] Optional real fuzz campaign evidence is independently recorded; it is not inferred from a unit-test PASS.
- [ ] Explain limited coverage and preserve any actual failure evidence.

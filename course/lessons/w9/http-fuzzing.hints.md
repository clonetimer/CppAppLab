# W9-A5 hints

## Hint 1 — Validate state before reading lengths

Check the enum, hidden request fields and close policy first. Use a bounded consumed prefix only after verifying the length.

## Hint 2 — Test the oracle itself

A parser may return consistent but wrong values. The synthetic test observations each violate one contract. Compare every request field and map content rather than map traversal order.

## Hint 3 — Separate fixed acceptance and fuzz evidence

Use the fixed replay driver and checked-in seeds. The optional real campaign must report actual runs and coverage counters; a missing toolchain, timeout or unexpected result cannot be accepted as evidence. Preserve a genuinely produced reproducer without targeting an external service.

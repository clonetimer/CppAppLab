## Hint 1 — Separate the path before the query

Validate the raw target first. Accept only a leading-slash origin-form path or exactly the OPTIONS asterisk-form. Split at the first question mark; do not treat a later question mark as another separator.

## Hint 2 — Use one percent-decoder with an explicit policy

Decode each %HH exactly once. Reject malformed escapes, NUL, control bytes and DEL. For path components only, reject percent-encoded slash and backslash; do not reject literal slash because it separates path segments.

## Hint 3 — Preserve query semantics

Split query pairs on ampersand and each pair on its first equals sign. Keep repeated names in an ordered vector. Missing equals means an empty value; plus remains plus. Reject empty names/segments and enforce the parameter/component limits before appending.

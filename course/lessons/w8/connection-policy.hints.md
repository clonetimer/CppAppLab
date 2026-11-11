## Hint 1 — Treat Connection as a list of options

A Connection field can contain comma-separated options and optional spaces or tabs. Do not search the raw string for the substring "close"; "x-close" is a different token.

## Hint 2 — Normalize tokens, not the whole request

Compare field names and option tokens with ASCII case-insensitive matching. Trim only optional whitespace around each token. Examine every Connection entry because manually constructed requests may contain case-variant keys.

## Hint 3 — Keep serialization authoritative

HTTP/1.1 defaults to Keep-Alive; HTTP/1.0 requires an explicit keep-alive option. The close token wins. Add policy-aware serializer overloads, filter caller-provided Connection and Content-Length fields, and preserve the legacy one-argument overload as close-after-response.

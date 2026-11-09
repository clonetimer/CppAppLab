## Hint 1 — Request line

Use `std::istringstream` to extract method, target and version. Then attempt to extract a fourth token; if one exists, reject the line. Parse into local variables first so a failed parse does not partially overwrite the request.

## Hint 2 — Response status line

Start with `HTTP/1.1 <status_code> <reason_phrase>\r\n`. Reject status codes outside 100–599 and reject CR or LF in the reason phrase.

## Hint 3 — Header handling

Validate header names as HTTP token characters. Header names are case-insensitive, so normalize names for duplicate detection and for the special Content-Length and Connection fields.

## Hint 4 — Content-Length

Use `response.body.size()`. The value is a byte count. For UTF-8 text, byte length can be larger than the number of displayed characters.

## Hint 5 — Safe serialization

Ignore caller-supplied Content-Length and Connection values in this first task. Emit the computed Content-Length and `Connection: close` exactly once. End headers with `\r\n\r\n`, then append the body unchanged.

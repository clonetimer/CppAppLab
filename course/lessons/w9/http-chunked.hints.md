## Hint 1 — Keep the framing decision separate

Find the transfer-encoding and content-length headers before reading the body. Reject both together. For this task, accept only one coding token equal to chunked after ASCII case-insensitive comparison and optional whitespace trimming.

## Hint 2 — Parse the body as a sequence of bounded records

Use a cursor into the original string_view. Parse the hexadecimal size line with std::from_chars(..., 16). For each nonzero chunk, wait for payload bytes plus CRLF. Track the cumulative decoded body size before appending.

## Hint 3 — Finish only at the empty trailer line

After the zero-sized chunk, validate each trailer name/value, reject framing/routing fields and case-insensitive duplicates, and stop only at the final CRLF. Return the cursor after that line as consumed; do not consume a following request.

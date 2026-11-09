## Hint 1 — Parser result

Do not return bool. Use three states: NeedMoreData, Complete and Error. Keep consumed at zero unless one full request is complete.

## Hint 2 — Header boundary

Search for `\r\n\r\n`. If absent and the buffer is within the 16 KiB limit, return NeedMoreData. If it exceeds the limit, return Error.

## Hint 3 — Header parsing

Split each header line at the first colon. Trim the name and value, lowercase the name, and reject empty names or duplicate normalized names.

## Hint 4 — Content-Length

Use `std::from_chars` into `std::size_t`. Require no conversion error and require the returned pointer to equal the end of the value. Reject values above 1 MiB.

## Hint 5 — Body and consumed bytes

The body starts immediately after the CRLFCRLF terminator. If fewer than Content-Length bytes remain, return NeedMoreData. Otherwise copy exactly that many bytes and set consumed to header bytes plus body bytes.

## Hint 6 — Pipelining

Do not consume bytes after the first complete request. The caller removes `result.consumed` bytes and invokes the parser again on the remaining buffer.

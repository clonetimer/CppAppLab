## Hint 1 — Reuse D1

Do not rewrite the HTTP message model. Carry forward the validated D1 `HttpRequest`, `HttpResponse` and `serialize_response` implementation. The serializer owns Content-Length and Connection: close.

## Hint 2 — Read bytes

Open files with `std::ifstream(path, std::ios::binary)`. Stream into a string and check for read failure. Do not use text-mode line reading for arbitrary assets.

## Hint 3 — Status handling

- Non-GET: 405 Method Not Allowed and `Allow: GET`.
- Unsafe target: 400 Bad Request.
- Missing/unreadable file: 404 Not Found.
- Successful read: 200 OK.

## Hint 4 — URL target

Remove the query string before file lookup. Map `/` to `/index.html`. For this learning stage, require a leading slash and reject `..` and backslash.

## Hint 5 — MIME types

Use `std::filesystem::path::extension()`. Map common web types and fall back to `application/octet-stream`.

## Hint 6 — Keep responsibilities separate

The handler returns an `HttpResponse`. It should not write to a socket or manually build HTTP wire bytes; the serializer performs that job.

## Hint 1 — Store callable handlers

Use `std::function<HttpResponse(const HttpRequest&)>`. Keep method and path as separate lookup dimensions. Re-registering an existing pair replaces its handler.

## Hint 2 — Validate before replacement

Reject empty/invalid method tokens, non-rooted or query-bearing registration paths and empty callbacks before touching an existing registration. Method and path matching are case-sensitive.

## Hint 3 — Match path, preserve target

For a request, lookup uses `request.target.substr(0, request.target.find('?'))`. Pass the unmodified request to the callback. Do not pretend this parses query parameters or decodes URLs.

## Hint 4 — Distinguish 404 and 405

A missing dynamic path is 404. For a known path with no matching method, collect the registered methods, sort them and join them into Allow. Return 405; do not try a same-named static file.

## Hint 5 — Three handlers

Hello returns the course greeting with a final LF. Echo copies the string body by byte count, including embedded NUL. Health returns the fixed JSON document with application/json. Register GET /hello, POST /echo and GET /health.

## Hint 6 — Dispatch by path ownership

Check `has_path` after extracting the path. Dynamic paths return `router.handle(request)` even when the handler responds 404. Only absent dynamic paths use `static_files.handle(request)`. Serialization and network I/O remain outside Router.

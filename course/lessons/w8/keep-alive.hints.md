## Hint 1 — Extend Connection state

Save the request's ConnectionPolicy and HTTP version before moving the request to the worker. Keep the unconsumed bytes in read_buffer after parsing one request.

## Hint 2 — A response must finish before the next request starts

Disable EPOLLIN while a handler is running or a response remains to be written. Keep EPOLLOUT enabled only while bytes remain. When the response is fully sent, close or dispatch the next buffered request.

## Hint 3 — Treat EOF and pipelines as state transitions

A peer half-close does not erase complete buffered requests. Drain them in order, close after the final response, and reject an incomplete request at EOF. Never run pipelined handlers concurrently on the same connection.

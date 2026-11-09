## Hint 1 — IPv4 socket setup

Create an `AF_INET` / `SOCK_STREAM` descriptor. Fill `sockaddr_in`, convert the port with `htons`, convert the IPv4 text with `inet_pton`, then call `bind` and `listen`.

## Hint 2 — Discover an ephemeral port

After binding port zero, call `getsockname` on the listening descriptor. Convert `sin_port` back with `ntohs`.

## Hint 3 — Accept and echo

`accept` returns a separate connected descriptor. Repeatedly `recv` into a bounded buffer and pass exactly the received byte count to `send_all`. Stop when `recv` returns zero.

## Hint 4 — Complete writes

Maintain an offset. On a positive `send`, advance it; on `EINTR`, retry; on zero or another error, fail. Use `MSG_NOSIGNAL` on Linux so a disconnected peer does not terminate the process with SIGPIPE.

## Hint 5 — Ownership

Use a small move-only descriptor guard internally. The listener owns the listening descriptor; each accepted connection owns its client descriptor; the caller owns the descriptor returned by `connect_tcp`.

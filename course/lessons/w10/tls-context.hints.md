# W10-A1 hints

## Hint 1 — Own the handles

Create the correct client/server SSL_CTX method inside the supplied Context type. Keep ownership on all error paths; set the minimum version and disable compression.

## Hint 2 — Establish explicit identity and trust

Load the server certificate and key, then check the pair. Load the client's explicit CA file and configure SSL_VERIFY_PEER. Missing or incompatible files return the documented typed error, never an insecure fallback.

## Hint 3 — Bind the name to the session

After checking the context and peer-name arguments, create an owned SSL session. Set client state, expected hostname and SNI; disallow partial-label wildcard matching. The fixed BIO driver tests actual handshakes and verifies wrong-name and wrong-CA rejection. It is not a real network listener.

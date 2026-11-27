# W10 · A1 TLS Context, Certificate Trust and Ownership

## Start here

1. Read `include/tls_context.hpp` and the move-only Context/Session aliases.
2. Implement only `src/tls_context.cpp`. First create a server context; then load its local certificate and private key.
3. Configure the client with its explicit CA file, TLS version floor and peer verification; bind the expected DNS identity and SNI to the session.
4. Run the fixed tests. Compare a real trusted handshake, a wrong hostname and an unrelated CA. No network server needs to be started.
5. Explain ownership, verification and the separate transport state machine needed in W10-A2.

## Goal

Learn RAII around OpenSSL 3.x handles, certificate/key consistency, explicit client trust and hostname verification. The fixture performs real TLS handshakes and binary data transfer over a memory BIO pair. All keys and short-lived certificates are generated locally in the build directory by the platform; never use production credentials. This is a context/identity component, not a deployed TLS service.

## Your task

Edit only `src/tls_context.cpp`. The header and test certificate generation are platform-owned.

| Function | Contract |
|---|---|
| make_server_context | TLS server context; minimum TLS 1.2, disable compression, load PEM certificate chain/private key and require their match |
| make_client_context | TLS client context; same version floor, disable compression, load only the explicit trust file, require SSL_VERIFY_PEER |
| make_client_session | verified client context, nonempty DNS name without embedded NUL (at most 253 bytes), client state, expected hostname and SNI; reject a server or unverified context |

Return a null owned handle and typed error on failure. Empty or embedded-NUL arguments are InvalidArgument; missing/inconsistent server identity is IdentityLoad; unreadable trust file is TrustLoad. Distinguish allocation and protocol/name setup errors. Do not disable peer verification or add fallback trust. The fixture tests localhost and an unrelated synthetic hostname only, not arbitrary network targets. Server-side client certificate authentication, full DNS syntax validation, socket event scheduling, connection deadlines, shutdown and WebSocket remain separate tasks.

## Build and run

Ubuntu 22.04/24.04 uses C++17, GCC, CMake, GoogleTest plus OpenSSL 3.x development headers and the openssl command. Install missing dependencies through the normal setup path; do not change the original C++ standard. In the workbench use Build, Run / Test and Formal Acceptance. The fixed CMake fixture generates disposable CA/leaf/key files in the build directory and runs 17 real tests. It does not listen on a network port.

## Explain your design

Why is a valid certificate chain insufficient without verifying the expected hostname? Who owns SSL_CTX and SSL, and why should errors never return a half-configured handle? How does SSL_get_error distinguish a temporary WANT_READ/WANT_WRITE from a fatal handshake failure? Why does a real memory BIO handshake not establish epoll transport correctness?

## Troubleshooting

- Certificate or private key load fails: examine the typed result and verify the PEM file and key pair; do not turn verification off.
- Unrelated CA or wrong DNS name succeeds: check SSL_VERIFY_PEER, SSL_CTX_load_verify_locations and SSL_set1_host.
- Handshake needs more input: the fixed driver alternates the two peers; do not treat WANT_READ/WANT_WRITE as a fatal error.
- Linker cannot find OpenSSL: install libssl-dev and openssl through the supported Ubuntu setup path, not a new global compiler.
- Tests leave handles or leak memory: keep the supplied move-only RAII ownership on every error path.

## Acceptance

- [ ] All 17 fixed tests pass using actual OpenSSL, GCC and GoogleTest.
- [ ] Client trusts only the explicit local CA; wrong identity and unrelated trust anchor are rejected.
- [ ] Contexts enforce TLS 1.2 minimum; actual TLS 1.2 negotiation and binary record transfer work.
- [ ] The initial implementation fails and the correct implementation passes the actual fixed learning-engine check.
- [ ] Explain the limited in-memory fixture and next transport-stage boundary.

# Release Qualification Checklist

## Release policy

A green hosted CI run is necessary but not sufficient. Do not create a release tag until the exact source commit has passed the repository CI gates, offline source-bundle verification and target Windows 11 + WSL2 acceptance.

## Gate A — repository evidence

- [ ] All intended feature work is merged into \`main\`.
- [ ] The final commit SHA is recorded.
- [ ] GCC/GoogleTest, ASan/UBSan, formatting and clang-tidy jobs pass on that exact commit.
- [ ] All fixed HTTP fixtures pass: protocol, parser, static files, Router, TCP server, connection policy, Keep-Alive, connection guards, graceful shutdown, observability and operational load/soak.
- [ ] The roadmap, status, known limitations and first-use documentation match the merged code.

## Gate B — reproducible offline source package

- [ ] Generate the offline source bundle from a clean committed checkout.
- [ ] Verify the SHA-256 sidecar before transfer.
- [ ] Confirm the manifest identifies the exact source commit and GoogleTest v1.18.0.
- [ ] Extract the bundle on a network-disconnected Ubuntu 24.04 WSL2 instance.
- [ ] Run \`bash scripts/check_environment.sh --strict-wsl\`.
- [ ] Run \`bash scripts/build_offline.sh\` with network access disabled.
- [ ] Record the complete CTest result and bundle SHA-256.

The bundle contains CppAppLab source and GoogleTest source. It does not include the Ubuntu distribution or GCC/CMake/GDB system packages.

## Gate C — target Windows 11 + WSL2 acceptance

Record these values from the actual target machine:

- Windows version and WSL version (\`wsl --status\`, \`wsl -l -v\`)
- Ubuntu \`/etc/os-release\` and \`uname -r\`
- \`g++ --version\`, \`cmake --version\`, \`gdb --version\`
- \`clang-format --version\`, \`clang-tidy --version\`
- repository commit SHA and offline bundle SHA-256

Then verify:

- [ ] Ubuntu distribution is version 24.04 and runs under WSL2.
- [ ] The strict environment checker passes.
- [ ] CMake configure and build pass from the offline bundle.
- [ ] Root CTest passes.
- [ ] GDB can break at \`main\` and continue execution.
- [ ] The HTTP server reference workspace builds and serves a real loopback request.
- [ ] All four D6-D3 operational acceptance cases pass.
- [ ] VS Code opens the checkout in a WSL: Ubuntu remote session.

## Gate D — release artifact and declaration

- [ ] Source/offline bundle and SHA-256 are archived.
- [ ] Release notes list supported environment, exact tested commit, test evidence and limitations.
- [ ] Known limitations are explicit: Linux-only implementation, no TLS, no HTTP/2/3, no chunked transfer encoding, and bounded/cooperative handler shutdown.
- [ ] No unverified platform compatibility or production-capacity claim is made.
- [ ] Only after Gates A–D pass: create the version tag and publish the release record.

## Current state

P7 is in progress. Hosted Ubuntu CI is not a substitute for Gate C. No Release is declared until the target Windows 11 + WSL2 acceptance evidence is supplied and recorded.

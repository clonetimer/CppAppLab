# CP7-D2b · Frozen archive replay (preparation)

Parent candidate: #96, `b706866428afb7d37af7c9ae582ff2a858db550d`. D2a completed capture and packaging steps at run [46000267714](https://github.com/clonetimer/CppAppLab/actions/runs/46000267714); full relocated course gate still running at this checkpoint. Do not replace or cancel #96's tested commit.

## Prepared implementation

- Require a reviewed frozen lock matching the exact committed version baseline and its SHA-256. Require the capture run, artifact and report identity. Reject draft capture status, changes in the 63-package set, unsafe filenames, foreign URLs, malformed hashes or sizes.
- Acquire exact archives from the official MSYS2 package server, verify their locked sizes and SHA-256 before installing, require detached signatures. Use a clean Windows CI runner and one local-only pacman transaction with Required TrustedOnly, no repository sections, no dependency bypass or untrusted-signature exception. Verify the installed set again after installation. This is consistent with [MSYS2 archive installation](https://www.msys2.org/docs/package-management/) and deliberately stricter than the [default local-file signature policy](https://github.com/msys2/MSYS2-packages/blob/master/pacman/pacman.conf).
- Record a replay receipt, not a fabricated acceptance result. Pure fixture tests cover candidate-status rejection, version drift, source validation, corrupt bytes and local-only signature configuration.

## Remaining activation gate

Do not enable in the delivery workflow yet. Obtain the actual D2a report from the successful artifact and verify its identity against the captured version baseline; freeze those exact hashes (no invented values), retarget this change onto merged main, then wire the clean runner acquisition, package manifest and relocated 101-test gate. Review corresponding source and redistribution independently. Windows replay is **NOT RUN**, and no release approval is claimed.

## D2a report reviewed and D2b activated on the candidate branch

Run 46000267714 completed successfully. Its independent report artifact 15197867836 was downloaded, the outer ZIP SHA-256 verified against GitHub metadata (`28d51969731c261ba67c1cd09aaf28c1e01a3be6002dcbc833bd6f604a8779a9`), ZIP CRC checked, and inner JSON hashed (`ca621cad3cfef1719401ed3ee386fd2f5841921b74133a623cd68f6f11794a7c`). All 63 versions, filenames, official URLs, positive sizes and SHA-256 fields were checked, the reconstructed exact version baseline SHA-256 matched `feec673608f34615700ac1eb2ada57e75635471fbc004e3a005422acc65e1c3a`. The frozen file's independently generated Git blob SHA `ff7793cc7070eaf214d1ea842fa304c633d88da7` matched the repository object byte-for-byte. Locked archives total 151654649 bytes; frozen lock SHA-256 `1118f65f6e4c0a3c0ec367785ffca648ddd629d38ec8fe03740f58ce3b13fd29`.

Candidate workflow now prepares only MSYS2 base/keyring, replays the actual locked archives in one local-only signature-enforced transaction and checks all installed versions; package creation and extraction require the frozen lock plus replay receipt and retain all original 101 course checks. No actual replay PASS until new Windows CI. No general reproducible-build claim: bootstrap MSYS2/keyring and CI environment remain variable; unavailable upstream archive/signature must fail closed. Corresponding source and redistribution review remain separate.

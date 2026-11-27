# CP7-D2b · Frozen archive replay (preparation)

Parent candidate: #96, `b706866428afb7d37af7c9ae582ff2a858db550d`. D2a completed capture and packaging steps at run [46000267714](https://github.com/clonetimer/CppAppLab/actions/runs/46000267714); full relocated course gate still running at this checkpoint. Do not replace or cancel #96's tested commit.

## Prepared implementation

- Require a reviewed frozen lock matching the exact committed version baseline and its SHA-256. Require the capture run, artifact and report identity. Reject draft capture status, changes in the 63-package set, unsafe filenames, foreign URLs, malformed hashes or sizes.
- Acquire exact archives from the official MSYS2 package server, verify their locked sizes and SHA-256 before installing, require detached signatures. Use a clean Windows CI runner and one local-only pacman transaction with Required TrustedOnly, no repository sections, no dependency bypass or untrusted-signature exception. Verify the installed set again after installation. This is consistent with [MSYS2 archive installation](https://www.msys2.org/docs/package-management/) and deliberately stricter than the [default local-file signature policy](https://github.com/msys2/MSYS2-packages/blob/master/pacman/pacman.conf).
- Record a replay receipt, not a fabricated acceptance result. Pure fixture tests cover candidate-status rejection, version drift, source validation, corrupt bytes and local-only signature configuration.

## Remaining activation gate

Do not enable in the delivery workflow yet. Obtain the actual D2a report from the successful artifact and verify its identity against the captured version baseline; freeze those exact hashes (no invented values), retarget this change onto merged main, then wire the clean runner acquisition, package manifest and relocated 101-test gate. Review corresponding source and redistribution independently. Windows replay is **NOT RUN**, and no release approval is claimed.

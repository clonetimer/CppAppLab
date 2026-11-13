# P7-C — Windows 11 + WSL2 target acceptance

This acceptance procedure is for a Windows 11 machine that already has WSL2 and either Ubuntu 22.04 LTS or Ubuntu 24.04 LTS. It does not install Ubuntu or enable WSL. The optional offline dependency step installs only the selected C++ build/debug toolchain from the matching local package bundle.

## 1. Confirm the existing WSL distribution

In Windows PowerShell:

~~~powershell
wsl --list --verbose
~~~

The source-build target may be Ubuntu 22.04 or Ubuntu 24.04 and must report WSL version 2. If neither is present, stop here; the acceptance script does not create or install a distribution. Learner and offline dependency packages are built separately for each release.

## 2. Select the delivery type

The prebuilt learner package and offline dependency bundle are built separately for Ubuntu 22.04 LTS x86_64 and Ubuntu 24.04 LTS x86_64. Select the archive whose filename matches the installed WSL distribution. Download the learner and offline dependency artifacts from the matching Ubuntu 22.04 or Ubuntu 24.04 jobs in the latest successful CppAppLab CI run. The two package families are separate artifacts; retain their filenames and SHA-256 sidecars.

For either Ubuntu release, extract the matching learner archive on Windows or copy it into the existing WSL distribution. Keep the exact archive filename and its SHA-256 sidecar/manifest. The package name includes `ubuntu22.04` or `ubuntu24.04`; do not use a package built for the other release.

## 3. Run the acceptance script inside the existing Ubuntu distribution

For Ubuntu 22.04 or Ubuntu 24.04, copy the repository's `scripts/accept_learner_package_wsl.sh` into the existing WSL checkout and run the matching prebuilt-package check:

~~~bash
bash scripts/accept_learner_package_wsl.sh /path/to/CppAppLab-learner-ubuntu22.04-x86_64-*.tar.gz
# Or, on Ubuntu 24.04:
bash scripts/accept_learner_package_wsl.sh /path/to/CppAppLab-learner-ubuntu24.04-x86_64-*.tar.gz
~~~

The script uses a temporary HOME and state directory. It:

1. Checks the packaged CLI version against BUILD-INFO.txt.
2. Discovers W1, W8 and W9 tasks from the packaged course catalog.
3. Creates a W1 learner workspace and runs the fixed Hello World acceptance check.
4. Checks the saved progress and acceptance history.
5. Moves the extracted package to a different directory containing spaces.
6. Re-runs the CLI from the relocated directory and verifies that the W1 PASS, progress and history remain available.

The script does not call apt, sudo, wsl --install, or any package installer. It requires the existing Ubuntu distribution to already have the compiler needed by the W1 acceptance fixture.

## 4. Verify or install the matching offline dependency bundle

If using the offline dependency archive, select the one matching the current distribution:

- Ubuntu 22.04: `CppAppLab-offline-deps-ubuntu22.04-amd64-*.tar.gz`
- Ubuntu 24.04: `CppAppLab-offline-deps-ubuntu24.04-amd64-*.tar.gz`

Extract the archive and verify it without changing installed packages:

~~~bash
tar -xzf CppAppLab-offline-deps-ubuntu22.04-amd64-*.tar.gz
cd CppAppLab-offline-deps-ubuntu22.04-amd64-*/
./install.sh --verify-only
~~~

Use the corresponding `ubuntu24.04` filename on Ubuntu 24.04. To install the bundled tools, run `sudo bash install.sh`; add `--with-quality-tools` to include clang-format and clang-tidy. The installer rejects a bundle built for a different Ubuntu release and disables network downloads during installation.

## 5. Verify the Windows double-click launcher

From the existing Windows checkout, double-click Start-CppAppLab.cmd or run it from PowerShell. The launcher must select the existing Ubuntu distribution and open the CppAppLab learner home. To exercise setup mode after a deliberate source update, run:

~~~powershell
.\Start-CppAppLab.cmd -Setup
~~~

The setup mode rebuilds the source checkout and runs the project's configured tests; it does not install an Ubuntu distribution.

## Evidence to record

- Windows version and WSL version from wsl --list --verbose
- Exact Ubuntu distribution name
- Learner archive filename and SHA-256
- CLI version and source commit from BUILD-INFO.txt
- W1 acceptance output
- Progress/history before and after package relocation
- Windows launcher result

The hosted CI verifies Windows PowerShell 5.1/7 parsing and Windows-to-WSL argument handoff with a stub WSL executable. That is not a substitute for this real target-machine WSL2 runtime check.

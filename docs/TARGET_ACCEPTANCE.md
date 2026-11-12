# P7-C — Windows 11 + WSL2 target acceptance

This acceptance procedure is for a Windows 11 machine that already has WSL2 and an Ubuntu 24.04 distribution. It does not install Ubuntu, enable WSL, or install Linux packages.

## 1. Confirm the existing WSL distribution

In Windows PowerShell:

~~~powershell
wsl --list --verbose
~~~

The target distribution must be Ubuntu 24.04 and report WSL version 2. If it is absent, stop here; the acceptance script does not create or install a distribution.

## 2. Download the P7 learner package

Download the source/learner package artifact from the P7 CI run:

https://github.com/clonetimer/CppAppLab/actions/runs/43149538317

Extract the learner archive on Windows or copy it into the existing WSL distribution. Keep the exact archive filename and its SHA-256 sidecar/manifest.

## 3. Run the acceptance script inside the existing Ubuntu distribution

Copy the repository's scripts/accept_learner_package_wsl.sh into the existing WSL checkout, then run:

~~~bash
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

## 4. Verify the Windows double-click launcher

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

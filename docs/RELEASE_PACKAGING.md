# Release packaging

## Package types

P7 produces two separate archives from one exact Git commit.

### Source package

- Generated from git archive HEAD.
- Contains the complete tracked source tree, engineering tests, course content, CI definitions and development documentation.
- Does not contain .git, build directories, generated test output or local learner state.
- Can be rebuilt with the repository's documented Ubuntu 24.04 toolchain.

### Learner package

- Contains a prebuilt Release CLI for Ubuntu 24.04 LTS x86_64.
- Contains the course catalog, lessons, progressive hints, platform-owned acceptance fixtures and learner workspace templates.
- Contains the environment checker and first-use instructions.
- Uses a small wrapper that sets CPPAPPLAB_DATA_DIR to the extracted package directory. The package can therefore be moved after extraction without rebuilding the platform CLI.
- Does not include the platform engine's C++ source tree or root developer test suite.

The labs tree includes reference baselines used by multi-stage fixed acceptance fixtures. They remain separate from learner templates and are not copied into learner workspaces. These files are part of the self-study package, not a security boundary.

## Supported target

- Ubuntu 24.04 LTS x86_64
- Windows 11 through WSL2 running Ubuntu 24.04
- GCC/C++17 and CMake for learner acceptance builds
- GoogleTest development package for CMake/GoogleTest task fixtures
- GDB for the environment and debugging lessons

The learner package contains the prebuilt CppAppLab CLI, but learner acceptance fixtures still compile the learner's C++ source locally. It is not a compiler or operating-system dependency bundle.

## Build packages locally

From the repository root:

~~~bash
cmake -S . -B build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DCPPAPPLAB_FETCH_GTEST=OFF
cmake --build build-release --parallel
bash scripts/package_release.sh build-release/cppapplab dist
(cd dist && sha256sum --check SHA256SUMS)
bash scripts/smoke_learner_package.sh dist/CppAppLab-learner-ubuntu24.04-x86_64-v0.1.0-<commit>.tar.gz
~~~

The package manifest records the exact source commit, target platform, package names and SHA-256 checksums. Archive metadata is normalized using the source commit timestamp and gzip's no-name mode.

## CI package candidates

The CI package job builds both archives, verifies the checksum manifest, extracts the learner package into a temporary directory, and tests the relocated CLI. The smoke test discovers W1/W8/W9 tasks, creates a W1 learner workspace from the packaged template, and passes its fixed Hello World acceptance check.

CI uploads the source archive, learner archive, package manifest and SHA256SUMS as a workflow artifact with 14-day retention. These are package candidates, not a versioned GitHub Release. A formal release is created only after offline dependency delivery and Windows 11 + WSL2 target-machine acceptance are complete.

## Offline dependencies

P7-B builds a separate Ubuntu 24.04 amd64 dependency archive. Required learner/build packages are separated from optional quality tools:

- Required: build-essential, CMake, Git, GDB and libgtest-dev.
- Optional: clang-format and clang-tidy.

The resolver uses the existing Ubuntu 24.04 APT package indexes and the read-only apt-get --print-uris mode to resolve and download the complete .deb closure. Bundle generation does not run apt-get update and does not install packages on the build host. The package lock records exact package names, versions, architectures, dependencies, sizes and SHA-256 values.

The bundled installer verifies all checksums and installs from local .deb files with --no-download; it never runs apt-get update. --verify-only checks integrity and package metadata without APT operations. --dry-run invokes APT simulation with downloads disabled and makes no system changes.

The archive targets an existing Ubuntu 24.04 amd64 installation, including WSL2 Ubuntu 24.04. It does not install an Ubuntu operating system. Clean-base package installation remains a target-machine acceptance step and is not claimed by the no-install CI checks.

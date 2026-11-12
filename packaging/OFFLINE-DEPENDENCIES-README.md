# CppAppLab offline dependency bundle

Target: an existing Ubuntu 24.04 LTS x86_64 (amd64) installation, including Ubuntu 24.04 under WSL2.

**This bundle does not install an Ubuntu operating system.** It supplies the C++ build and debugging tools for an Ubuntu 24.04 installation that already exists.

## Included packages

Required learner/build toolchain:

- build-essential (GCC/G++)
- CMake
- Git
- GDB
- GoogleTest development headers and libraries

Optional code-quality tools:

- clang-format
- clang-tidy

The package builder refreshes only the ephemeral CI runner's APT package indexes from a pinned Ubuntu archive snapshot, then resolves the dependency closure using APT's read-only --print-uris mode and downloads the corresponding .deb files. It does not run apt-get install, does not modify the dpkg database, and does not install packages. The exact snapshot ID is recorded in BUILD-INFO.txt and packages.lock.json.

## Verify without installing

Extract the archive and run:

~~~bash
tar -xzf CppAppLab-offline-deps-ubuntu24.04-amd64-*.tar.gz
cd CppAppLab-offline-deps-ubuntu24.04-amd64-*/
./install.sh --verify-only
./install.sh --dry-run
./install.sh --dry-run --with-quality-tools
~~~

The dry-run uses APT simulation with --no-download; it does not change installed packages or access the network.

## Install on an existing Ubuntu 24.04 system

Required learner/build tools:

~~~bash
sudo bash install.sh
~~~

Optional quality tools as well:

~~~bash
sudo bash install.sh --with-quality-tools
~~~

The installer verifies SHA-256 checksums and installs only from the bundled local .deb files. APT downloads are explicitly disabled. No apt-get update is run.

## Package records

- PACKAGES.txt: required and optional package groups
- packages.lock.json: exact package names, versions, architectures, dependencies, sizes and SHA-256 values
- SHA256SUMS: per-file integrity manifest
- BUILD-INFO.txt: source commit and package resolution method

The package closure is resolved against the Ubuntu 24.04 amd64 repository metadata available at build time. The target system must already be Ubuntu 24.04 amd64 and have a functioning apt/dpkg installation.

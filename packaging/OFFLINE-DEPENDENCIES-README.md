# CppAppLab offline dependency bundle

Target: Ubuntu 24.04 LTS x86_64 (amd64), including WSL2 Ubuntu 24.04.

This bundle contains the Ubuntu .deb files for:

- build-essential (GCC/G++)
- CMake
- Git
- GDB
- GoogleTest development headers and libraries
- clang-format
- clang-tidy

The package builder resolves and downloads the complete dependency closure from a clean Ubuntu 24.04 base image. The installation script verifies SHA-256 checksums and installs only from the bundled local .deb files. It explicitly disables package downloads.

## Install

Extract the archive and run:

~~~bash
tar -xzf CppAppLab-offline-deps-ubuntu24.04-amd64-*.tar.gz
cd CppAppLab-offline-deps-ubuntu24.04-amd64-*/
sudo bash install.sh
~~~

The target machine must already have a functioning Ubuntu 24.04 base installation with apt/dpkg. No external network connection is used by install.sh.

## Verify

~~~bash
g++ --version
cmake --version
git --version
gdb --version
clang-format --version
clang-tidy --version
dpkg-query -W libgtest-dev
~~~

See PACKAGES.txt and SHA256SUMS for the exact downloaded package set and integrity values. BUILD-INFO.txt records the source commit used to build this bundle.

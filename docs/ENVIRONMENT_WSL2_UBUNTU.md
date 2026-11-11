# Development Environment: WSL2 + Ubuntu 24.04

This is the complete acceptance environment for CppAppLab v1.

## 1. Verify WSL2 from Windows

Open PowerShell:

```powershell
wsl --status
wsl -l -v
```

The Ubuntu distribution must show `VERSION 2`. If it shows version 1, convert it from an elevated or normal PowerShell session as appropriate:

```powershell
wsl --set-version Ubuntu 2
```

Start Ubuntu:

```powershell
wsl -d Ubuntu
```

## 2. Verify Ubuntu

Inside Ubuntu:

```bash
cat /etc/os-release
uname -r
```

The target distribution is Ubuntu 24.04 LTS and the kernel should identify the WSL2 environment.

## 3. Install the toolchain

```bash
sudo apt update
sudo apt install -y \\
  build-essential \\
  cmake \\
  git \\
  gdb \\
  clang-format \\
  clang-tidy \\
  libgtest-dev
```

The required compiler is GCC/g++. The course baseline is C++17. CMake must be at least 3.18.

## 4. Verify tool versions

```bash
g++ --version
cmake --version
git --version
gdb --version
clang-format --version
clang-tidy --version
```

## 5. Run the environment checker

From a CppAppLab checkout:

```bash
bash scripts/check_environment.sh
```

For a strict WSL2 check:

```bash
bash scripts/check_environment.sh --strict-wsl
```

The checker compiles and runs a small C++17 program and asks GDB to stop at `main`. It checks Ubuntu 24.04, the WSL2 kernel, GCC 11+, CMake 3.18+, GDB, CTest and GoogleTest. In strict mode it also requires clang-format and clang-tidy.

## 6. Build CppAppLab

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCPPAPPLAB_FETCH_GTEST=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/cppapplab --version
./build/cppapplab --help
```

If you have extracted the CppAppLab offline source bundle, use `bash scripts/build_offline.sh`; it builds against the bundled GoogleTest source with network FetchContent disabled. See [Offline Build](OFFLINE_BUILD.md). If no bundle is available, the default CMake configuration can fetch GoogleTest v1.18.0 and requires network access.

## 7. VS Code

Install VS Code on Windows and the WSL extension. From the Ubuntu project directory run:

```bash
code .
```

The VS Code status bar should indicate a WSL: Ubuntu remote session. Keep active C++ work in the Linux filesystem (for example, `~/work/CppAppLab`) rather than under `/mnt/c` unless Windows-file interoperability is specifically needed.

## 8. Offline source build

On a connected machine, generate the offline source bundle with `bash scripts/create_offline_bundle.sh`. Transfer the archive and its `.sha256` sidecar to the WSL2 filesystem, verify the checksum, extract it, then run `bash scripts/check_environment.sh --strict-wsl` and `bash scripts/build_offline.sh`. The bundle includes CppAppLab source and GoogleTest source, not Ubuntu or system toolchain packages. See [Offline Build](OFFLINE_BUILD.md) for the complete procedure.

## 9. Environment acceptance checklist

- [ ] PowerShell reports Ubuntu distribution version 2.
- [ ] Ubuntu reports version 24.04.
- [ ] GCC 11+, CMake 3.18+, `git`, `gdb`, and `ctest` are available.
- [ ] `clang-format` and `clang-tidy` are available.
- [ ] System or bundled GoogleTest is available.
- [ ] The environment checker passes.
- [ ] CMake configure and build pass.
- [ ] CTest passes.
- [ ] The network-disabled offline build passes from the source bundle.
- [ ] GDB can break at `main`.
- [ ] VS Code opens the project in WSL: Ubuntu.

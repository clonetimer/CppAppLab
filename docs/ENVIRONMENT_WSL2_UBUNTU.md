# Development Environment: WSL2 + Ubuntu 24.04

This is the complete acceptance environment for CppAppLab v1.

For the shortest first-use path, clone the repository in Ubuntu and run `bash scripts/quickstart.sh`. The script checks required packages, offers to install missing dependencies, builds the platform, runs its tests, and starts the guided first task. The detailed manual setup remains below.

## 1. Verify WSL2 from Windows

Open PowerShell:

```powershell
wsl --status
wsl -l -v
```

The Ubuntu distribution must show `VERSION 2`. If it shows version 1, convert it from an elevated or normal PowerShell session as appropriate:

```powershell
wsl --set-version <distribution-name> 2
```

Start Ubuntu:

```powershell
wsl -d <distribution-name>
```

Use the exact distribution name shown by `wsl -l -v` (for example, `Ubuntu` or `Ubuntu-24.04`).

## Windows launcher

After the repository is available in Windows Explorer, `Start-CppAppLab.cmd` provides a direct Windows entry point. It detects the installed Ubuntu distribution, verifies WSL2 and Ubuntu 24.04, clones into `~/work/CppAppLab` only when that checkout is missing, and opens the interactive learning home. Use `Start-CppAppLab.cmd -Setup` after updating source code to rebuild and rerun CTest. The launcher does not update an existing Git checkout automatically.

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

The checker compiles and runs a small C++17 program and asks GDB to stop at `main`. It also checks the distribution and required tools.

## 6. Build CppAppLab

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCPPAPPLAB_FETCH_GTEST=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/cppapplab --version
./build/cppapplab --help
```

If the system GoogleTest package is unavailable, omit `-DCPPAPPLAB_FETCH_GTEST=OFF` and allow CMake to fetch the pinned dependency. The first fetch requires network access.

## 7. VS Code

Install VS Code on Windows and the WSL extension. From the Ubuntu project directory run:

```bash
code .
```

The VS Code status bar should indicate a WSL: Ubuntu remote session. Keep active C++ work in the Linux filesystem (for example, `~/work/CppAppLab`) rather than under `/mnt/c` unless Windows-file interoperability is specifically needed.

## 8. Environment acceptance checklist

- [ ] PowerShell reports Ubuntu distribution version 2.
- [ ] Ubuntu reports version 24.04.
- [ ] `g++`, `cmake`, `git`, and `gdb` are available.
- [ ] The environment checker passes.
- [ ] CMake configure and build pass.
- [ ] CTest passes.
- [ ] GDB can break at `main`.
- [ ] VS Code opens the project in WSL: Ubuntu.

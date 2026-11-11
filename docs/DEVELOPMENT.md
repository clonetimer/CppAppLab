# Development and Acceptance

## Local build

From the repository root in Ubuntu:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/cppapplab --help
```

If GoogleTest is installed from Ubuntu packages, configure with `-DCPPAPPLAB_FETCH_GTEST=OFF`. For an offline build, extract the source bundle and run `bash scripts/build_offline.sh`; it uses the bundled GoogleTest v1.18.0 source and disables CMake package discovery and FetchContent network access.

## Offline source bundle

On a connected, clean checkout run:

```bash
bash scripts/create_offline_bundle.sh "$HOME/CppAppLab-offline.tar.gz"
sha256sum -c "$HOME/CppAppLab-offline.tar.gz.sha256"
```

Transfer the archive and checksum to the target WSL2 filesystem, extract the package, then run `bash scripts/check_environment.sh --strict-wsl` and `bash scripts/build_offline.sh`. The bundle includes source and GoogleTest source only; install Ubuntu/GCC/CMake/GDB system packages separately. See [Offline Build](OFFLINE_BUILD.md).

## Sanitizer build

```bash
cmake -S . -B build-sanitize \\
  -DCMAKE_BUILD_TYPE=Debug \\
  -DCPPAPPLAB_ENABLE_SANITIZERS=ON
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

## Quality checks

```bash
find include src tests -type f \\( -name '*.cpp' -o -name '*.hpp' \\) -print0 \\
  | xargs -0 clang-format --dry-run --Werror

cmake -S . -B build-quality \\
  -DCMAKE_BUILD_TYPE=Debug \\
  -DCPPAPPLAB_ENABLE_CLANG_TIDY=ON
cmake --build build-quality --parallel
ctest --test-dir build-quality --output-on-failure
```

## Target WSL2 and release qualification

Use [Release Qualification](RELEASE_QUALIFICATION.md) as the release gate. Hosted CI is not a substitute for running the strict environment checker and offline build on the actual Windows 11 + WSL2 + Ubuntu 24.04 target machine. Record the target machine's toolchain versions, tested commit SHA, CTest counts and offline bundle SHA-256 before declaring a release.

## Contribution workflow

1. Inspect current `main`, open issues, and active pull requests.
2. Create a focused feature branch from the current `main`.
3. Make one coherent change; keep learner-facing material and platform code clearly separated.
4. Run configure, build, CTest, relevant sanitizer checks, formatting, and static analysis.
5. Record exact commands, toolchain versions, test counts, and known limitations.
6. Open a pull request with scope, evidence, risks, and follow-up work.
7. Merge only after review and required CI checks pass.
8. Tag only a tested, reproducible milestone. Do not infer release readiness from a successful compile alone.

## Acceptance rules

- Do not silently skip missing acceptance tests.
- Do not count reference code as learner completion.
- Do not trust learner-modified tests as the acceptance authority.
- A changed source fingerprint invalidates the current-pass status.
- CI validates repository behavior; it does not certify human understanding.
- Report untested operating systems and compiler versions explicitly.

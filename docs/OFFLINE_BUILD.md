# Offline Source Bundle and Build

## Scope

The offline bundle contains the committed CppAppLab source tree and the pinned GoogleTest v1.18.0 source. It allows CMake configure, compile and CTest to run without network access after extraction.

It does **not** contain Ubuntu, GCC, CMake, GDB, Git, clang-format, clang-tidy or their operating-system packages. Install the target toolchain before moving to an offline machine.

## 1. Generate the bundle on a connected machine

Use a clean, committed checkout on Ubuntu 24.04:

\`\`\`bash
git status --short
# Commit or remove all changes before packaging.

bash scripts/create_offline_bundle.sh "$HOME/CppAppLab-offline.tar.gz"
\`\`\`

The generator:

1. Fetches GoogleTest from the pinned \`v1.18.0\` tag.
2. Exports the exact committed CppAppLab source tree.
3. Places GoogleTest under \`third_party/googletest\`.
4. Writes \`OFFLINE_BUNDLE_MANIFEST.txt\`.
5. Produces a SHA-256 sidecar.

The script refuses to package a dirty working tree so the archive always identifies one source commit.

## 2. Transfer and verify

Transfer both files to the target WSL2 Ubuntu filesystem:

- \`CppAppLab-offline.tar.gz\`
- \`CppAppLab-offline.tar.gz.sha256\`

Verify the archive before extraction:

\`\`\`bash
sha256sum -c CppAppLab-offline.tar.gz.sha256
mkdir -p "$HOME/work/CppAppLab-offline"
tar -xzf CppAppLab-offline.tar.gz -C "$HOME/work/CppAppLab-offline"
cd "$HOME/work/CppAppLab-offline/package"
\`\`\`

## 3. Build with network access disabled

Run the strict target-environment check, then build:

\`\`\`bash
bash scripts/check_environment.sh --strict-wsl
bash scripts/build_offline.sh
\`\`\`

The offline build explicitly:

- disables \`find_package(GTest)\` so a system package cannot mask the bundled dependency
- points \`CPPAPPLAB_GTEST_SOURCE_DIR\` at \`third_party/googletest\`
- sets \`FETCHCONTENT_FULLY_DISCONNECTED=ON\`
- configures, builds and runs the full root CTest suite

No CMake FetchContent network fallback is available in this mode.

An alternate build directory can be supplied:

\`\`\`bash
bash scripts/build_offline.sh "$HOME/build/cppapplab-offline"
\`\`\`

## 4. Online/system-package build remains supported

If \`libgtest-dev\` is installed from Ubuntu packages:

\`\`\`bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCPPAPPLAB_FETCH_GTEST=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure
\`\`\`

If neither a system GoogleTest package nor the offline source bundle is available, the default CMake configuration can fetch the pinned dependency and therefore requires network access.

## Acceptance boundary

A successful offline build proves that the source package and GoogleTest dependency are sufficient for a reproducible Linux build. It does not certify the user's Windows 11 + WSL2 installation, GDB workflow, system package provenance, or release readiness. Record the bundle SHA-256, manifest commit, toolchain versions and target-machine test results separately.

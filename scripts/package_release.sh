#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BINARY="$ROOT/build-release/cppapplab"
OUTPUT="$ROOT/dist"
if [[ $# -ge 1 && -n "$1" ]]; then BINARY="$1"; fi
if [[ $# -ge 2 && -n "$2" ]]; then OUTPUT="$2"; fi

if [[ "$BINARY" != /* ]]; then BINARY="$ROOT/$BINARY"; fi
if [[ "$OUTPUT" != /* ]]; then OUTPUT="$ROOT/$OUTPUT"; fi

if [[ ! -x "$BINARY" ]]; then
    echo "[FAIL] Release binary is missing or not executable: $BINARY" >&2
    exit 2
fi
if ! git -C "$ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "[FAIL] Package creation requires a Git checkout." >&2
    exit 2
fi

VERSION="$(awk '$1 == "VERSION" { print $2; exit }' "$ROOT/CMakeLists.txt")"
COMMIT="$(git -C "$ROOT" rev-parse HEAD)"
SHORT_SHA="$(printf '%s' "$COMMIT" | cut -c1-12)"
SOURCE_DATE_EPOCH="$(git -C "$ROOT" log -1 --format=%ct HEAD)"
if [[ -z "$VERSION" || -z "$COMMIT" ]]; then
    echo "[FAIL] Cannot determine the package version or source commit." >&2
    exit 2
fi

mkdir -p "$OUTPUT"
OUTPUT="$(cd "$OUTPUT" && pwd)"
TEMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TEMP_ROOT"' EXIT

SOURCE_NAME="CppAppLab-source-v$VERSION-$SHORT_SHA"
LEARNER_NAME="CppAppLab-learner-ubuntu24.04-x86_64-v$VERSION-$SHORT_SHA"
SOURCE_ARCHIVE="$OUTPUT/$SOURCE_NAME.tar.gz"
LEARNER_ARCHIVE="$OUTPUT/$LEARNER_NAME.tar.gz"

echo "[INFO] Creating source archive: $(basename "$SOURCE_ARCHIVE")"
git -C "$ROOT" archive --format=tar --prefix="$SOURCE_NAME/" HEAD | gzip -n > "$SOURCE_ARCHIVE"

LEARNER_ROOT="$TEMP_ROOT/$LEARNER_NAME"
mkdir -p "$LEARNER_ROOT/bin" "$LEARNER_ROOT/scripts" "$LEARNER_ROOT/docs"

install -m 0755 "$BINARY" "$LEARNER_ROOT/bin/cppapplab.bin"
cp -a "$ROOT/course" "$LEARNER_ROOT/course"
cp -a "$ROOT/labs" "$LEARNER_ROOT/labs"
install -m 0644 "$ROOT/README.md" "$LEARNER_ROOT/README-REPOSITORY.md"
install -m 0644 "$ROOT/docs/QUICKSTART.md" "$LEARNER_ROOT/docs/QUICKSTART.md"
install -m 0644 "$ROOT/docs/ENVIRONMENT_WSL2_UBUNTU.md" "$LEARNER_ROOT/docs/ENVIRONMENT_WSL2_UBUNTU.md"
install -m 0755 "$ROOT/scripts/check_environment.sh" "$LEARNER_ROOT/scripts/check_environment.sh"
install -m 0644 "$ROOT/packaging/LEARNER-README.md" "$LEARNER_ROOT/README.md"

cat > "$LEARNER_ROOT/cppapplab" <<'WRAPPER'
#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
export CPPAPPLAB_DATA_DIR="$ROOT"
exec "$ROOT/bin/cppapplab.bin" "$@"
WRAPPER
chmod 0755 "$LEARNER_ROOT/cppapplab"

cat > "$LEARNER_ROOT/BUILD-INFO.txt" <<EOF
Project: CppAppLab
Version: $VERSION
Source commit: $COMMIT
Build target: Ubuntu 24.04 LTS x86_64
Build configuration: Release, BUILD_TESTING=OFF
Package type: relocatable learner package
Reference baselines: included for platform-owned multi-stage acceptance fixtures
EOF

find "$LEARNER_ROOT" -type f -exec touch -d "@$SOURCE_DATE_EPOCH" {} +
find "$LEARNER_ROOT" -type d -exec touch -d "@$SOURCE_DATE_EPOCH" {} +

echo "[INFO] Creating learner archive: $(basename "$LEARNER_ARCHIVE")"
tar --sort=name \
    --mtime="@$SOURCE_DATE_EPOCH" \
    --owner=0 --group=0 --numeric-owner \
    -C "$TEMP_ROOT" -cf - "$LEARNER_NAME" | gzip -n > "$LEARNER_ARCHIVE"

SOURCE_SHA256="$(sha256sum "$SOURCE_ARCHIVE" | awk '{print $1}')"
LEARNER_SHA256="$(sha256sum "$LEARNER_ARCHIVE" | awk '{print $1}')"

cat > "$OUTPUT/package-manifest.json" <<EOF
{
  "schema_version": 1,
  "project": "CppAppLab",
  "version": "$VERSION",
  "source_commit": "$COMMIT",
  "target": "Ubuntu 24.04 LTS x86_64",
  "packages": {
    "source": {
      "file": "$(basename "$SOURCE_ARCHIVE")",
      "sha256": "$SOURCE_SHA256"
    },
    "learner": {
      "file": "$(basename "$LEARNER_ARCHIVE")",
      "sha256": "$LEARNER_SHA256",
      "prebuilt_cli": true,
      "includes_course_fixtures": true,
      "includes_reference_baselines": true
    }
  }
}
EOF

(
    cd "$OUTPUT"
    sha256sum "$(basename "$SOURCE_ARCHIVE")" \
        "$(basename "$LEARNER_ARCHIVE")" \
        package-manifest.json > SHA256SUMS
)

echo "[PASS] Release packages created."
echo "  Source:  $SOURCE_ARCHIVE"
echo "  Learner: $LEARNER_ARCHIVE"
echo "  Manifest: $OUTPUT/package-manifest.json"
echo "  Checksums: $OUTPUT/SHA256SUMS"

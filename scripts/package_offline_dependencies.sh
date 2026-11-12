#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUTPUT="$ROOT/dist"
if [[ $# -ge 1 && -n "$1" ]]; then OUTPUT="$1"; fi
if [[ "$OUTPUT" != /* ]]; then OUTPUT="$ROOT/$OUTPUT"; fi

for tool in apt-get curl python3 dpkg-deb sha256sum tar gzip; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "[FAIL] Required preinstalled tool is missing: $tool" >&2
        exit 2
    fi
done
if [[ ! -r /etc/os-release ]]; then
    echo "[FAIL] Cannot identify the package-build host." >&2
    exit 2
fi
# shellcheck disable=SC1091
source /etc/os-release
if [[ "$ID" != "ubuntu" || "$VERSION_ID" != "24.04" ]]; then
    echo "[FAIL] Package resolution targets Ubuntu 24.04; detected ${PRETTY_NAME:-unknown}." >&2
    exit 2
fi
if [[ "$(dpkg --print-architecture)" != "amd64" ]]; then
    echo "[FAIL] Package resolution targets amd64." >&2
    exit 2
fi
if ! git -C "$ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "[FAIL] Offline dependency packaging requires a Git checkout." >&2
    exit 2
fi

VERSION="$(awk '$1 == "VERSION" { print $2; exit }' "$ROOT/CMakeLists.txt")"
COMMIT="$(git -C "$ROOT" rev-parse HEAD)"
SHORT_SHA="$(printf '%s' "$COMMIT" | cut -c1-12)"
SOURCE_DATE_EPOCH="$(git -C "$ROOT" log -1 --format=%ct HEAD)"
PACKAGE_NAME="CppAppLab-offline-deps-ubuntu24.04-amd64-v${VERSION}-${SHORT_SHA}"

mkdir -p "$OUTPUT"
OUTPUT="$(cd "$OUTPUT" && pwd)"
TEMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TEMP_ROOT"' EXIT
DOWNLOAD_ROOT="$TEMP_ROOT/downloads"
BUNDLE_ROOT="$TEMP_ROOT/$PACKAGE_NAME"
mkdir -p "$DOWNLOAD_ROOT" "$BUNDLE_ROOT/packages/base" "$BUNDLE_ROOT/packages/quality"

resolve_uris() {
    local label="$1"
    shift
    local empty_status="$TEMP_ROOT/${label}.status"
    local apt_log="$TEMP_ROOT/${label}-apt-uris.log"
    local uri_json="$TEMP_ROOT/${label}-uris.json"
    : > "$empty_status"

    echo "[INFO] Resolving ${label} dependency closure in read-only APT URI mode." >&2
    if ! sudo apt-get \
        -o "Dir::State::status=$empty_status" \
        --print-uris --yes --no-install-recommends install "$@" > "$apt_log" 2>&1; then
        cat "$apt_log" >&2
        echo "[FAIL] APT URI resolution failed. No package installation was attempted." >&2
        return 1
    fi
    if ! python3 "$ROOT/packaging/resolve_apt_uris.py" --input "$apt_log" --output "$uri_json" >&2; then
        echo "[FAIL] APT --print-uris output could not be parsed; raw resolver output follows:" >&2
        cat "$apt_log" >&2
        return 1
    fi
    printf '%s\n' "$uri_json"
}

download_uri_json() {
    local uri_json="$1"
    local entries="$TEMP_ROOT/$(basename "$uri_json").tsv"
    python3 - "$uri_json" > "$entries" <<'PY'
import json
import sys
for entry in json.load(open(sys.argv[1], encoding="utf-8")):
    print(f"{entry['uri']}\t{entry['filename']}\t{entry['size']}\t{entry.get('sha256', '')}")
PY
    while IFS=$'\t' read -r uri filename expected_size expected_sha; do
        local target="$DOWNLOAD_ROOT/$filename"
        if [[ ! -f "$target" ]]; then
            echo "[INFO] Downloading $filename"
            curl --fail --location --retry 3 --retry-delay 2 \
                --output "$target" "$uri"
        fi
        local actual_size
        actual_size="$(stat -c '%s' "$target")"
        if [[ "$actual_size" != "$expected_size" ]]; then
            echo "[FAIL] Size mismatch for $filename: expected $expected_size, got $actual_size." >&2
            return 1
        fi
        if [[ -n "$expected_sha" ]]; then
            local actual_sha
            actual_sha="$(sha256sum "$target" | awk '{print $1}')"
            if [[ "$actual_sha" != "$expected_sha" ]]; then
                echo "[FAIL] APT SHA-256 mismatch for $filename." >&2
                return 1
            fi
        fi
    done < "$entries"
}

mapfile -t base_roots < <(grep -Ev '^[[:space:]]*(#|$)' "$ROOT/packaging/offline-dependencies.txt" | sed 's/[[:space:]]//g')
mapfile -t quality_roots < <(grep -Ev '^[[:space:]]*(#|$)' "$ROOT/packaging/offline-quality-tools.txt" | sed 's/[[:space:]]//g')
if [[ "${#base_roots[@]}" -eq 0 || "${#quality_roots[@]}" -eq 0 ]]; then
    echo "[FAIL] Required or optional quality package list is empty." >&2
    exit 2
fi

if ! base_json="$(resolve_uris required "${base_roots[@]}")"; then
    exit 1
fi
if ! full_json="$(resolve_uris full "${base_roots[@]}" "${quality_roots[@]}")"; then
    exit 1
fi
download_uri_json "$base_json"
download_uri_json "$full_json"

copy_group() {
    local uri_json="$1"
    local destination="$2"
    python3 - "$uri_json" "$DOWNLOAD_ROOT" "$destination" <<'PY'
import json
import shutil
import sys
from pathlib import Path
entries = json.load(open(sys.argv[1], encoding="utf-8"))
source = Path(sys.argv[2])
target = Path(sys.argv[3])
target.mkdir(parents=True, exist_ok=True)
for entry in entries:
    shutil.copy2(source / entry["filename"], target / entry["filename"])
PY
}

copy_group "$base_json" "$BUNDLE_ROOT/packages/base"
python3 - "$base_json" "$full_json" "$DOWNLOAD_ROOT" "$BUNDLE_ROOT/packages/quality" <<'PY'
import json
import shutil
import sys
from pathlib import Path
base = {entry["filename"] for entry in json.load(open(sys.argv[1], encoding="utf-8"))}
full = json.load(open(sys.argv[2], encoding="utf-8"))
source = Path(sys.argv[3])
target = Path(sys.argv[4])
target.mkdir(parents=True, exist_ok=True)
for entry in full:
    if entry["filename"] not in base:
        shutil.copy2(source / entry["filename"], target / entry["filename"])
PY

install -m 0644 "$ROOT/packaging/OFFLINE-DEPENDENCIES-README.md" "$BUNDLE_ROOT/README.md"
install -m 0755 "$ROOT/scripts/install_offline_dependencies.sh" "$BUNDLE_ROOT/install.sh"
cp "$ROOT/packaging/offline-dependencies.txt" "$BUNDLE_ROOT/REQUIRED-PACKAGES.txt"
cp "$ROOT/packaging/offline-quality-tools.txt" "$BUNDLE_ROOT/OPTIONAL-QUALITY-PACKAGES.txt"
printf 'amd64\n' > "$BUNDLE_ROOT/ARCHITECTURE"

cat > "$BUNDLE_ROOT/BUILD-INFO.txt" <<EOF
Project: CppAppLab
Version: $VERSION
Source commit: $COMMIT
Target: Ubuntu 24.04 LTS amd64
Required roots: ${base_roots[*]}
Optional quality roots: ${quality_roots[*]}
Dependency resolver: apt-get --print-uris with an empty dpkg status database
Package installation performed during bundle creation: no
Archive target: existing Ubuntu 24.04 installation; this bundle does not install an Ubuntu operating system
EOF

python3 - "$BUNDLE_ROOT" <<'PY'
import json
import subprocess
import sys
from pathlib import Path
root = Path(sys.argv[1])
packages = []
for group in ("base", "quality"):
    for deb in sorted((root / "packages" / group).glob("*.deb")):
        def field(name):
            try:
                return subprocess.check_output(
                    ["dpkg-deb", "-f", str(deb), name],
                    text=True, stderr=subprocess.DEVNULL
                ).strip()
            except subprocess.CalledProcessError:
                return ""
        package = {
            "group": group,
            "file": f"packages/{group}/{deb.name}",
            "name": field("Package"),
            "version": field("Version"),
            "architecture": field("Architecture"),
            "depends": field("Depends"),
            "pre_depends": field("Pre-Depends"),
            "size": deb.stat().st_size,
            "sha256": subprocess.check_output(["sha256sum", str(deb)], text=True).split()[0],
        }
        if not package["name"] or not package["version"]:
            raise SystemExit(f"Invalid Debian package metadata: {deb}")
        if package["architecture"] not in ("amd64", "all"):
            raise SystemExit(f"Unexpected package architecture {package['architecture']}: {deb}")
        packages.append(package)

identities = [(item["group"], item["name"], item["version"]) for item in packages]
if len(identities) != len(set(identities)):
    raise SystemExit("Duplicate package identity found in the offline bundle.")
for required in ("build-essential", "cmake", "git", "gdb", "libgtest-dev"):
    if not any(item["name"] == required and item["group"] == "base" for item in packages):
        raise SystemExit(f"Required package {required} is missing.")
for optional in ("clang-format", "clang-tidy"):
    if not any(item["name"] == optional and item["group"] == "quality" for item in packages):
        raise SystemExit(f"Optional quality package {optional} is missing.")

manifest = {
    "schema_version": 1,
    "project": "CppAppLab",
    "target": "Ubuntu 24.04 LTS amd64",
    "install_policy": "local deb files only; apt downloads disabled",
    "package_installation_performed_during_build": False,
    "packages": packages,
}
(root / "packages.lock.json").write_text(
    json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
)
(root / "PACKAGES.txt").write_text(
    "Required learner/build packages:\n"
    + "\n".join(f"  - {name}" for name in ("build-essential", "cmake", "git", "gdb", "libgtest-dev"))
    + "\n\nOptional quality tools:\n"
    + "\n".join(f"  - {name}" for name in ("clang-format", "clang-tidy"))
    + "\n\nExact package names, versions, architectures, dependencies and hashes are in packages.lock.json.\n",
    encoding="utf-8",
)
print(f"[PASS] Inspected {len(packages)} Debian packages; no packages were installed.")
PY

find "$BUNDLE_ROOT" -type f -exec touch -d "@$SOURCE_DATE_EPOCH" {} +
find "$BUNDLE_ROOT" -type d -exec touch -d "@$SOURCE_DATE_EPOCH" {} +
(
    cd "$BUNDLE_ROOT"
    find . -type f -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
)

ARCHIVE="$OUTPUT/$PACKAGE_NAME.tar.gz"
tar --sort=name \
    --mtime="@$SOURCE_DATE_EPOCH" \
    --owner=0 --group=0 --numeric-owner \
    -C "$TEMP_ROOT" -cf - "$PACKAGE_NAME" | gzip -n > "$ARCHIVE"
sha256sum "$ARCHIVE" > "$ARCHIVE.sha256"

echo "[PASS] Offline dependency bundle created without installing Ubuntu packages."
echo "  Required .deb count: $(find "$BUNDLE_ROOT/packages/base" -maxdepth 1 -type f -name '*.deb' | wc -l)"
echo "  Optional .deb count: $(find "$BUNDLE_ROOT/packages/quality" -maxdepth 1 -type f -name '*.deb' | wc -l)"
echo "  Archive: $ARCHIVE"
echo "  Size: $(stat -c '%s' "$ARCHIVE") bytes"

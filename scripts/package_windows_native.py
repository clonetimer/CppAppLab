"""Build a Windows x64 portable CI candidate from a real native build.

The archive owns the observed UCRT64 package bytes and official embedded
Python, not a system MSYS2 installation. Publication is a separate gate.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
from urllib.parse import quote
import zipfile

PYTHON_VERSION = "3.14.10"
PYTHON_URL = (
    "https://www.python.org/ftp/python/3.14.10/python-3.14.10-embed-amd64.zip"
)
PYTHON_SHA256 = "90905e64a840557d8c1bc4f5b74db3d7ee85e3ffb217611cac1c7813acea4159"
REQUIRED_PACKAGES = {
    "mingw-w64-ucrt-x86_64-" + name
    for name in ("gcc", "cmake", "ninja", "gtest", "gdb", "libgcc", "libstdc++", "libwinpthread")
}
REQUIRED_TOOLS = ("g++.exe", "gcc.exe", "cmake.exe", "ctest.exe", "ninja.exe",
                  "gdb.exe", "objdump.exe")
REQUIRED_RUNTIME_DLLS = ("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")
# This documented GCC split-package aggregator has no installed payload.
# Every concrete package still has to contribute its actual owned files.
METADATA_ONLY_PACKAGES = {
    "mingw-w64-ucrt-x86_64-cc-libs": {
        "mingw-w64-ucrt-x86_64-libgcc",
        "mingw-w64-ucrt-x86_64-libstdc++",
    }
}
CHUNK = 1024 * 1024


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def git(root: Path, *args: str) -> bytes:
    return subprocess.check_output(["git", "-C", str(root), *args])


def fields(path: Path) -> dict[str, list[str]]:
    result: dict[str, list[str]] = {}
    key = None
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("%") and line.endswith("%"):
            key = line.strip("%")
            result[key] = []
        elif line and key is not None:
            result[key].append(line)
    return result


def safe_relative(name: str) -> str:
    path = PurePosixPath(name)
    if (not name or path.is_absolute() or path.as_posix() != name or
            any(part in {"", ".", ".."} or ":" in part or "\\" in part
                for part in path.parts)):
        raise RuntimeError(f"Unsafe archive entry: {name!r}")
    return name


class Archive:
    def __init__(self, zip_path: Path):
        self.zip = zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED,
                                   compresslevel=3, allowZip64=True)
        self.files: dict[str, dict] = {}
        self.names: set[str] = set()

    def add(self, name: str, source) -> None:
        name = safe_relative(name)
        folded = name.casefold()
        if folded in self.names:
            raise RuntimeError(f"Duplicate case-insensitive archive entry: {name}")
        self.names.add(folded)
        hash_value = hashlib.sha256()
        size = 0
        with self.zip.open(name, "w", force_zip64=True) as target:
            while data := source.read(CHUNK):
                hash_value.update(data)
                size += len(data)
                target.write(data)
        self.files[name] = {"sha256": hash_value.hexdigest(), "bytes": size}

    def file(self, name: str, source: Path, allowed_root: Path) -> None:
        # Package-owned links are materialized as regular bytes only if their
        # resolved target remains inside the original acquisition prefix.
        target = source.resolve(strict=True)
        if not target.is_file() or not target.is_relative_to(allowed_root.resolve()):
            raise RuntimeError(f"Unsafe package source: {source}")
        with target.open("rb") as stream:
            self.add(name, stream)

    def bytes(self, name: str, content: bytes) -> None:
        from io import BytesIO
        self.add(name, BytesIO(content))

    def finish(self, manifest: dict) -> None:
        manifest["files"] = self.files
        self.zip.writestr("package-manifest.json",
                          json.dumps(manifest, ensure_ascii=False, indent=2,
                                     sort_keys=True) + "\n")
        self.zip.close()


def package(root: Path, binary: Path, msys: Path, python_zip: Path, out: Path,
            acquisition_report: Path) -> Path:
    if os.name != "nt":
        raise RuntimeError("Native Windows build host required.")
    root, binary, msys, python_zip, out = (
        p.resolve(strict=True) if index != 4 else p.absolute()
        for index, p in enumerate((root, binary, msys, python_zip, out))
    )
    if digest(python_zip) != PYTHON_SHA256:
        raise RuntimeError("Official embedded Python ZIP SHA-256 mismatch.")
    lock = root / "packaging/windows/ucrt64-versions.json"
    baseline = json.loads(lock.read_text(encoding="utf-8"))
    report = json.loads(acquisition_report.read_text(encoding="utf-8"))
    if (report["schema_version"] != 1 or
            report["status"] != "ARCHIVE_HASH_CAPTURE_CANDIDATE" or
            report["version_baseline_sha256"] != digest(lock) or
            report["baseline"] != baseline["baseline"]):
        raise RuntimeError("Acquisition report does not match the frozen version baseline.")
    records = report["packages"]
    if ({name: value["version"] for name, value in records.items()} != baseline["packages"]
            or any(not re.fullmatch(r"[0-9a-f]{64}", value["sha256"])
                   or value["bytes"] <= 0 for value in records.values())):
        raise RuntimeError("Incomplete or mismatched acquisition archive hashes.")
    prefix = msys / "ucrt64"
    for name in REQUIRED_TOOLS:
        if not (prefix / "bin" / name).is_file():
            raise RuntimeError(f"Missing acquired native tool: {name}")
    if binary.suffix.lower() != ".exe":
        raise RuntimeError("Expected a real native cppapplab.exe.")
    # Verify the acquisition before allowing it into the native package.
    env = dict(os.environ, PATH=str(prefix / "bin") + os.pathsep + os.environ["PATH"])
    target = subprocess.check_output(
        [str(prefix / "bin/g++.exe"), "-dumpmachine"], env=env, text=True
    ).strip()
    if target != "x86_64-w64-mingw32":
        raise RuntimeError(f"Not a native x64 GCC target: {target}")
    objdump = str(prefix / "bin/objdump.exe")
    for exe in [binary] + [prefix / "bin" / name for name in REQUIRED_TOOLS]:
        fmt = subprocess.check_output([objdump, "-f", str(exe)], env=env, text=True)
        imports = subprocess.check_output([objdump, "-p", str(exe)], env=env, text=True)
        if "pei-x86-64" not in fmt or re.search(r"msys-2\.0\.dll|cygwin1\.dll",
                                               imports, re.I):
            raise RuntimeError(f"Non-native executable: {exe}")

    commit = git(root, "rev-parse", "HEAD").decode().strip()
    version_match = re.search(r"(?m)^\s*VERSION\s+([\d.]+)\s*$",
                              (root / "CMakeLists.txt").read_text(encoding="utf-8"))
    if not version_match:
        raise RuntimeError("Cannot identify repository version.")
    version = version_match.group(1)
    out.mkdir(parents=True, exist_ok=True)
    archive_path = out / f"CppAppLab-Windows-x64-v{version}-{commit[:12]}-candidate.zip"
    manifest = {
        "schema_version": 1, "project": "CppAppLab", "version": version,
        "source_commit": commit, "target": "Windows x64 (native UCRT64)",
        "status": "CI_CANDIDATE_NOT_PUBLIC_RELEASE",
        "python": {"version": PYTHON_VERSION, "url": PYTHON_URL,
                   "archive_sha256": PYTHON_SHA256,
                   "release_page": "https://www.python.org/downloads/release/python-31410/"},
        "toolchain_acquisition": "MSYS2 UCRT64 frozen version set; actual archive hash capture candidate",
        "acquisition_report_sha256": digest(acquisition_report),
        "toolchain_target": target,
        "toolchain_packages": [],
        "formal_task_ids": ["w1.hello-world", "w1.calculator", "w2.todo-cli",
                            "w3.task-manager", "w4.quality-gate"],
        "limits": ["No public redistribution approval yet.",
                   "W5-W9 need Linux/WSL2.",
                   "W2-W4 real file-symlink checks need Developer Mode or existing privilege.",
                   "Windows-native independent knowledge/modern projects pending.",
                   "Target-machine/human assessment not run."],
    }
    archive = Archive(archive_path)
    try:
        archive.file("bin/cppapplab.exe", binary, binary.parent)
        archive.bytes("provenance/acquisition-lock-candidate.json", acquisition_report.read_bytes())
        archive.bytes("provenance/ucrt64-versions.json", lock.read_bytes())
        tracked = git(root, "ls-files", "-z", "--",
                      "course", "labs", "cmake", "web/server", "web/ui").decode("utf-8")
        for name in sorted(filter(None, tracked.split("\0"))):
            if name.startswith("web/server/") and not name.endswith(".py"):
                continue
            archive.file(name, root / safe_relative(name), root)
        for source, destination in [
            ("packaging/windows/Start-CppAppLab.cmd", "Start-CppAppLab.cmd"),
            ("packaging/windows/cppapplab.cmd", "cppapplab.cmd"),
            ("packaging/windows/start-native.py", "start-native.py"),
            ("packaging/windows/README.md", "README.md"),
            ("README.md", "README-REPOSITORY.md"),
        ]:
            archive.file(destination, root / source, root)
        for name in ("LICENSE", "LICENSE.md", "COPYING", "NOTICE"):
            if (root / name).is_file():
                archive.file(name, root / name, root)

        with zipfile.ZipFile(python_zip) as embedded:
            if embedded.testzip() is not None:
                raise RuntimeError("Corrupt embedded Python archive.")
            for info in embedded.infolist():
                if info.is_dir():
                    continue
                with embedded.open(info) as source:
                    archive.add("runtime/" + safe_relative(info.filename), source)
        if "runtime/python.exe" not in archive.files or not any(
                name.startswith("runtime/LICENSE") for name in archive.files):
            raise RuntimeError("Embedded Python executable/license missing.")

        seen_packages = set()
        for record in sorted((msys / "var/lib/pacman/local").iterdir()):
            if not (record / "desc").is_file() or not (record / "files").is_file():
                continue
            desc = fields(record / "desc")
            name = desc.get("NAME", [""])[0]
            if not name.startswith("mingw-w64-ucrt-x86_64-"):
                continue
            pkg_version = desc["VERSION"][0]
            files = fields(record / "files").get("FILES", [])
            seen_packages.add(name)
            package_meta = {key.lower(): value for key, value in desc.items()
                            if key in {"NAME", "BASE", "VERSION", "ARCH", "LICENSE",
                                       "URL", "BUILDDATE", "DEPENDS", "PACKAGER"}}
            package_meta["installed_files"] = 0
            package_meta["upstream_package_index"] = "https://packages.msys2.org/packages/" + quote(name, safe="")
            archive.file(f"provenance/msys2/{name}/desc", record / "desc", msys)
            archive.file(f"provenance/msys2/{name}/files", record / "files", msys)
            for name_in_package in files:
                if not name_in_package.startswith("ucrt64/") or name_in_package.endswith("/"):
                    continue
                relative = safe_relative(name_in_package.removeprefix("ucrt64/"))
                archive.file("toolchain/" + relative, prefix / relative, prefix)
                package_meta["installed_files"] += 1
            package_meta["payload_kind"] = "installed_files"
            if not package_meta["installed_files"]:
                expected_dependencies = METADATA_ONLY_PACKAGES.get(name)
                declared_dependencies = {
                    re.split(r"[<>=]", item, maxsplit=1)[0]
                    for item in desc.get("DEPENDS", [])
                }
                if (expected_dependencies is None
                        or not expected_dependencies <= declared_dependencies
                        or any(not item.endswith("/") for item in files)):
                    raise RuntimeError(f"Unexpected empty installed package inventory: {name} {pkg_version}")
                package_meta["payload_kind"] = "metadata_only"
                print(f"[INFO] Retaining dependency-only package {name} {pkg_version} in provenance.")
            else:
                print(f"[INFO] Included {name} {pkg_version}: {package_meta['installed_files']} files.")
            manifest["toolchain_packages"].append(package_meta)
        actual_versions = {meta["name"][0]: meta["version"][0]
                           for meta in manifest["toolchain_packages"]}
        if actual_versions != baseline["packages"]:
            raise RuntimeError("Installed package inventory drifted after acquisition capture.")
        if REQUIRED_PACKAGES - seen_packages:
            raise RuntimeError(f"Missing package provenance: {REQUIRED_PACKAGES - seen_packages}")
        if not any(name.startswith("toolchain/share/licenses/") for name in archive.files):
            raise RuntimeError("Native acquisition has no collected license directory.")
        for tool in REQUIRED_TOOLS + REQUIRED_RUNTIME_DLLS:
            if "toolchain/bin/" + tool not in archive.files:
                raise RuntimeError(f"Required tool/runtime missing from installed file inventory: {tool}")

        for relative in ("include/wchar.h", "include/windows.h"):
            if "toolchain/" + relative not in archive.files:
                raise RuntimeError(f"Native header missing from installed package inventory: {relative}")

        archive.bytes("BUILD-INFO.txt", (
            f"CppAppLab {version}\nSource: {commit}\nTarget: Windows x64 UCRT64\n"
            f"Embedded Python: {PYTHON_VERSION}\nStatus: CI candidate, not a public release\n"
            "Exact installed package metadata and file digests: package-manifest.json\n"
        ).encode("utf-8"))
        archive.finish(manifest)
    except BaseException:
        archive.zip.close()
        archive_path.unlink(missing_ok=True)
        raise
    archive_sha = digest(archive_path)
    (out / "SHA256SUMS").write_text(
        f"{archive_sha}  {archive_path.name}\n", encoding="ascii", newline="\n")
    (out / "package-summary.json").write_text(json.dumps({
        key: value for key, value in manifest.items() if key != "files"
    } | {"archive": archive_path.name, "archive_sha256": archive_sha,
         "archive_bytes": archive_path.stat().st_size, "file_count": len(archive.files)},
        ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"[PASS] Native candidate archive: {archive_path.name}; "
          f"{len(archive.files)} files, SHA-256 {archive_sha}")
    return archive_path


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--msys2-root", type=Path, required=True)
    parser.add_argument("--python-zip", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--acquisition-report", type=Path, required=True)
    args = parser.parse_args()
    package(args.root, args.binary, args.msys2_root, args.python_zip, args.output,
            args.acquisition_report)

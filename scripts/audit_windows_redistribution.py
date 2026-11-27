"""Collect actual source/notice evidence for the exact verified Windows ZIP.

An evidence collection success is not a license conclusion or public-release
authorization. Never execute upstream recipes or substitute a newer source.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path, PurePosixPath
import re
import os
import subprocess
import tempfile
from urllib.parse import quote

from package_windows_native import digest, fields, safe_relative
from replay_windows_toolchain_lock import download, load_frozen_lock

SOURCE_INDEX = "https://repo.msys2.org/mingw/sources/"
PYTHON_SOURCE = "https://www.python.org/ftp/python/3.14.10/Python-3.14.10.tar.xz"
PYTHON_SOURCE_SHA256 = "de414a57119abef52784d8993e341d53aed635fc2b05c2607d9f98a2c5b72032"
MAX_SOURCE = 1024 * 1024 * 1024
SAFE_SOURCE_PART = re.compile(r"[A-Za-z0-9_.+~-]+")


def source_identity(meta: dict) -> str:
    bases = meta.get("base", [])
    versions = meta.get("version", [])
    if len(bases) != 1 or len(versions) != 1:
        raise ValueError("Missing unambiguous installed source base/version.")
    base, version = bases[0], versions[0].replace(":", "~")
    if not base.startswith("mingw-w64-") or not SAFE_SOURCE_PART.fullmatch(base) or not SAFE_SOURCE_PART.fullmatch(version):
        raise ValueError("Invalid installed source base/version.")
    return f"{base}-{version}.src.tar.zst"


def notice_path(name: str) -> bool:
    relative = PurePosixPath(name)
    leaf = relative.name.upper()
    if name.startswith("ucrt64/share/licenses/"):
        return True
    # Header/source names such as license_parser.h are not legal notices.
    # Outside the dedicated license directory, inventory only conventional
    # notice names with text/markup suffixes (or extensionless COPYING.*).
    if not re.match(r"^(LICENSE|COPYING|NOTICE|COPYRIGHT)(?:[._-]|$)", leaf):
        return False
    if relative.suffix.lower() in {".h", ".hpp", ".hh", ".hxx", ".c", ".cc",
                                  ".cpp", ".cxx", ".py", ".pyc", ".pyo",
                                  ".dll", ".exe", ".a", ".o", ".obj", ".lib",
                                  ".so", ".dylib", ".cmake", ".js", ".ts",
                                  ".rs", ".go", ".java", ".class", ".sh",
                                  ".cmd", ".bat", ".ps1"}:
        return False
    return (leaf in {"LICENSE", "COPYING", "NOTICE", "COPYRIGHT"} or
            relative.suffix.lower() in {".txt", ".md", ".rst", ".html", ".htm",
                                         ".license", ".lic", ".rtf"} or
            bool(re.fullmatch(r"(?:LICENSE|COPYING|NOTICE|COPYRIGHT)[._-](?:GPL|LGPL|AGPL|BSD|MIT|APACHE|RUNTIME|LIBRARY|LESSER)(?:[._-][0-9A-Z]+)*", leaf)))


def groups_and_notices(summary: dict, lock: dict, msys: Path) -> tuple[list[dict], dict]:
    expected = lock["packages"]
    items = summary["toolchain_packages"]
    actual = {entry["name"][0]: entry["version"][0] for entry in items}
    if len(actual) != len(items) or actual != {name: row["version"] for name, row in expected.items()}:
        raise RuntimeError("Package summary does not correspond to the frozen lock.")
    installed = {}
    for record in sorted((msys / "var/lib/pacman/local").iterdir()):
        desc_path = record / "desc"
        if desc_path.is_file():
            desc = fields(desc_path)
            name = desc.get("NAME", [""])[0]
            if name in expected:
                if name in installed:
                    raise RuntimeError(f"Duplicate installed metadata: {name}")
                installed[name] = (record, desc)
    if set(installed) != set(expected):
        raise RuntimeError("Missing installed package metadata.")
    rows, groups = [], {}
    prefix = (msys / "ucrt64").resolve()
    for item in sorted(items, key=lambda row: row["name"][0]):
        name = item["name"][0]
        record, desc = installed[name]
        if desc["VERSION"] != item["version"]:
            raise RuntimeError(f"Installed version drift: {name}")
        for key in ("BASE", "LICENSE"):
            if desc.get(key, []) != item.get(key.lower(), []):
                raise RuntimeError(f"Summary metadata drift: {name} {key}")
        notices = []
        for raw in fields(record / "files").get("FILES", []):
            if not raw.startswith("ucrt64/") or raw.endswith("/") or not notice_path(raw):
                continue
            relative = safe_relative(raw.removeprefix("ucrt64/"))
            path = (prefix / relative).resolve(strict=True)
            if not path.is_relative_to(prefix) or not path.is_file():
                raise RuntimeError(f"Unsafe notice file: {raw}")
            notices.append({"zip_path": "toolchain/" + relative,
                            "sha256": digest(path), "bytes": path.stat().st_size})
        row = {"name": name, "version": item["version"][0],
               "payload_kind": item["payload_kind"],
               "declared_licenses": item.get("license", []),
               "notice_files": notices, "binary_archive": expected[name]["filename"],
               "binary_sha256": expected[name]["sha256"],
               "source_archive": None, "mapping_status": "REVIEW_REQUIRED"}
        try:
            filename = source_identity(item)
            row["source_archive"] = filename
            row["mapping_status"] = "EXACT_BASE_VERSION"
            group = groups.setdefault(filename, {"filename": filename,
                "url": SOURCE_INDEX + quote(filename), "binary_packages": []})
            group["binary_packages"].append(name)
        except ValueError as exc:
            row["mapping_error"] = str(exc)
        rows.append(row)
    return rows, groups


def collect_source(group: dict, scratch: Path, msys: Path) -> None:
    archive = scratch / group["filename"]
    sig = archive.with_name(archive.name + ".sig")
    bin_dir = msys / "usr/bin"
    env = dict(os.environ, PATH=str(bin_dir) + os.pathsep + os.environ["PATH"],
               MSYSTEM="MSYS", LC_ALL="C")

    def posix(path: Path) -> str:
        return subprocess.check_output([str(bin_dir / "cygpath.exe"), "-u", str(path.resolve())],
                                       env=env, encoding="utf-8", timeout=15).strip()

    try:
        download(group["url"], archive, MAX_SOURCE)
        download(group["url"] + ".sig", sig, 1024 * 1024)
        group.update(bytes=archive.stat().st_size, sha256=digest(archive),
                     signature_sha256=digest(sig))
        verify = subprocess.run([str(bin_dir / "gpg.exe"), "--batch", "--no-auto-key-retrieve",
                    "--homedir", "/etc/pacman.d/gnupg", "--status-fd", "1", "--verify",
                    posix(sig), posix(archive)], env=env, stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT, encoding="utf-8", errors="replace", timeout=120)
        status = verify.stdout.splitlines()
        valid = [line.split()[2] for line in status if line.startswith("[GNUPG:] VALIDSIG ")]
        trusted = any(line.startswith(("[GNUPG:] TRUST_FULLY", "[GNUPG:] TRUST_ULTIMATE"))
                      for line in status)
        if verify.returncode or len(valid) != 1 or not trusted:
            raise RuntimeError("Source signature not verified with fully trusted signer: " +
                               verify.stdout[-2000:])
        group["signer_fingerprint"] = valid[0]
        # List, never unpack or execute the source recipe.
        listing = subprocess.check_output([str(bin_dir / "bsdtar.exe"), "-tf", posix(archive)],
                                          env=env, encoding="utf-8", errors="replace", timeout=120)
        paths = listing.splitlines()
        recipes = [name for name in paths if PurePosixPath(name).name == "PKGBUILD"]
        if not recipes:
            raise RuntimeError("Signed source archive has no visible PKGBUILD.")
        group.update(status="SIGNED_SOURCE_ARCHIVE_FETCHED", recipes=recipes,
                     member_count=len(paths))
    except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
        group.update(status="SOURCE_EVIDENCE_BLOCKED", error=str(exc)[-3000:])
    finally:
        archive.unlink(missing_ok=True)
        sig.unlink(missing_ok=True)


def audit(root: Path, msys: Path, summary_path: Path, output: Path) -> None:
    if os.name != "nt" or os.environ.get("GITHUB_ACTIONS") != "true":
        raise RuntimeError("Real source collection runs only on the Windows acquisition CI runner.")
    summary = json.loads(summary_path.read_text(encoding="utf-8"))
    frozen_path = root / "packaging/windows/ucrt64-archives.json"
    lock = load_frozen_lock(root / "packaging/windows/ucrt64-versions.json", frozen_path)
    if summary["frozen_archive_lock_sha256"] != digest(frozen_path):
        raise RuntimeError("Summary lock identity mismatch.")
    rows, groups = groups_and_notices(summary, lock, msys)
    python = {"version": "3.14.10", "url": PYTHON_SOURCE,
              "published_sha256": PYTHON_SOURCE_SHA256,
              "binary_archive_sha256": summary["python"]["archive_sha256"],
              "release_page": "https://www.python.org/downloads/release/python-31410/",
              "status": "SOURCE_EVIDENCE_BLOCKED"}
    if summary["python"]["version"] != python["version"]:
        raise RuntimeError("Embedded Python version changed.")
    with tempfile.TemporaryDirectory(prefix="cppapplab-source-audit-") as folder:
        scratch = Path(folder)
        for filename, group in sorted(groups.items()):
            collect_source(group, scratch, msys)
            print(f"[AUDIT] {filename}: {group['status']}", flush=True)
        try:
            source = scratch / "Python-3.14.10.tar.xz"
            download(PYTHON_SOURCE, source, 100 * 1024 * 1024)
            if digest(source) != PYTHON_SOURCE_SHA256:
                raise RuntimeError("Official Python source hash mismatch.")
            python.update(status="OFFICIAL_SOURCE_HASH_VERIFIED", bytes=source.stat().st_size)
        except (OSError, RuntimeError) as exc:
            python["error"] = str(exc)[-3000:]
    source_ok = sum(group["status"] == "SIGNED_SOURCE_ARCHIVE_FETCHED" for group in groups.values())
    covered = sum(row["source_archive"] is not None and
                  groups[row["source_archive"]]["status"] == "SIGNED_SOURCE_ARCHIVE_FETCHED"
                  for row in rows)
    project_license_files = [name for name in ("LICENSE", "LICENSE.md", "COPYING", "NOTICE")
                             if (root / name).is_file()]
    report = {"schema_version": 1, "status": "EVIDENCE_COLLECTED_PUBLIC_RELEASE_NOT_APPROVED",
              "source_commit": summary["source_commit"],
              "binary_zip_sha256": summary["archive_sha256"],
              "package_summary_sha256": digest(summary_path),
              "frozen_archive_lock_sha256": digest(frozen_path),
              "packages": rows, "source_groups": list(groups.values()), "embedded_python": python,
              "counts": {"binary_packages": len(rows), "exact_source_groups": len(groups),
                         "signed_source_groups_fetched": source_ok,
                         "binary_packages_with_fetched_sources": covered,
                         "missing_declared_license": sum(not row["declared_licenses"] for row in rows),
                         "no_package_owned_notice": sum(not row["notice_files"] for row in rows)},
              "project_license_files": project_license_files,
              "publication_blockers": [
                  "Owner must confirm project license; do not infer a license from public visibility.",
                  "Per-component license and exception/notice obligations require review.",
                  "Source archive signatures and recipes do not prove complete corresponding source.",
                  "Check bundled dependencies and build/installation scripts, including embedded Python.",
                  "Collected sources are temporary; arrange durable equivalent source availability before release.",
                  "Physical target and human feedback not yet completed.",
              ]}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
                      encoding="utf-8")
    print(f"[AUDIT] Actual source archives {source_ok}/{len(groups)}; "
          f"binary package coverage {covered}/{len(rows)}; public release NOT APPROVED.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--msys2-root", type=Path, required=True)
    parser.add_argument("--summary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    audit(args.root, args.msys2_root, args.summary, args.output)

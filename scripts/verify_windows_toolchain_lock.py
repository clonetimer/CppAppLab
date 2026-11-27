"""Capture exact upstream archive hashes for the verified Windows version set.

D2a is a fail-closed capture stage, not an immutable acquisition source:
the normal signed MSYS2 runner install must match the checked-in baseline
exactly. Pacman downloads the explicit matching archives without refreshing
or installing; verify the actual cached bytes against sync metadata.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
from urllib.parse import quote

from package_windows_native import fields, digest

PREFIX = "mingw-w64-ucrt-x86_64-"
REPOSITORY_URL = "https://repo.msys2.org/mingw/ucrt64/"


def installed_versions(msys: Path) -> dict[str, str]:
    versions = {}
    for record in sorted((msys / "var/lib/pacman/local").iterdir()):
        desc = record / "desc"
        if not desc.is_file():
            continue
        metadata = fields(desc)
        name = metadata.get("NAME", [""])[0]
        if name.startswith(PREFIX):
            if name in versions:
                raise RuntimeError(f"Duplicate installed package: {name}")
            versions[name] = metadata["VERSION"][0]
    return versions


def require_exact(expected: dict[str, str], actual: dict[str, str]) -> None:
    missing = sorted(expected.keys() - actual.keys())
    extra = sorted(actual.keys() - expected.keys())
    changed = {key: {"expected": expected[key], "actual": actual[key]}
               for key in sorted(expected.keys() & actual.keys())
               if expected[key] != actual[key]}
    if missing or extra or changed:
        raise RuntimeError("UCRT64 acquisition version drift: " +
                           json.dumps({"missing": missing, "extra": extra,
                                       "changed": changed}, sort_keys=True))


def parse_plan(output: str, expected: dict[str, str]) -> dict[str, dict]:
    records = {}
    for line in output.splitlines():
        if not line.strip():
            continue
        parts = line.split("|")
        if len(parts) != 5:
            raise RuntimeError(f"Unexpected pacman acquisition record: {line}")
        name, version, repo, filename, sha = parts
        if (name in records or repo != "ucrt64" or
                not re.fullmatch(r"[A-Za-z0-9_.+~-]+\.pkg\.tar\.(?:zst|xz)", filename)
                or not filename.startswith(name + "-")
                or not re.fullmatch(r"[0-9a-fA-F]{64}", sha)):
            raise RuntimeError(f"Invalid or duplicate acquisition record: {line}")
        records[name] = {"version": version, "repository": repo,
                         "filename": filename, "sha256": sha.lower(),
                         "url": REPOSITORY_URL + quote(filename)}
    require_exact(expected, {name: value["version"] for name, value in records.items()})
    return records


def verify_archives(records: dict[str, dict], cache: Path) -> None:
    for name, value in records.items():
        archive = cache / value["filename"]
        if not archive.is_file():
            raise RuntimeError(f"Exact upstream archive missing: {archive}")
        if digest(archive) != value["sha256"]:
            raise RuntimeError(f"Upstream archive SHA-256 mismatch: {name}")
        value["bytes"] = archive.stat().st_size
        if value["bytes"] <= 0:
            raise RuntimeError(f"Empty upstream archive: {name}")


def capture(msys: Path, lock: Path, output: Path) -> None:
    if os.name != "nt":
        raise RuntimeError("Capture requires the real Windows acquisition runner.")
    baseline = json.loads(lock.read_text(encoding="utf-8"))
    if baseline["schema_version"] != 1 or baseline["status"] != "OBSERVED_VERSION_LOCK":
        raise RuntimeError("Unsupported Windows toolchain baseline.")
    expected = baseline["packages"]
    if not expected or any(not name.startswith(PREFIX) for name in expected):
        raise RuntimeError("Invalid UCRT64 version baseline.")
    require_exact(expected, installed_versions(msys))
    pacman = msys / "usr/bin/pacman.exe"
    env = dict(os.environ, PATH=str(pacman.parent) + os.pathsep + os.environ["PATH"],
               MSYSTEM="MSYS", LC_ALL="C")
    targets = [f"ucrt64/{name}={version}" for name, version in sorted(expected.items())]
    options = [str(pacman), "--noconfirm", "--noprogressbar", "--color", "never"]
    # No -y, -u, --needed or --nodeps. Exact package versions are mandatory.
    planned = subprocess.check_output(
        options + ["-Sp", "--print-format", "%n|%v|%r|%f|%h", *targets],
        env=env, encoding="utf-8", timeout=120)
    records = parse_plan(planned, expected)
    subprocess.run(options + ["-Sw", "--cachedir", "/var/cache/pacman/pkg", *targets],
                   env=env, check=True, timeout=900)
    verify_archives(records, msys / "var/cache/pacman/pkg")
    require_exact(expected, installed_versions(msys))
    report = {"schema_version": 1, "status": "ARCHIVE_HASH_CAPTURE_CANDIDATE",
              "version_baseline_sha256": digest(lock),
              "baseline": baseline["baseline"],
              "packages": dict(sorted(records.items())),
              "limitations": [
                  "Exact installed versions and cached archive bytes verified against pacman sync metadata.",
                  "Hash values still require review and commit before immutable replay.",
                  "Independent corresponding-source/redistribution review not yet complete.",
              ]}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"[PASS] Exact acquisition versions and archive SHA-256: {len(records)} packages.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--msys2-root", type=Path, required=True)
    parser.add_argument("--lock", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    capture(args.msys2_root, args.lock, args.output)

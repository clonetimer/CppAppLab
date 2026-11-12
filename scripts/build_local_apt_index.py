#!/usr/bin/env python3
"""Build a flat local APT Packages index from bundled .deb files."""
from __future__ import annotations

import argparse
import gzip
import hashlib
import subprocess
from pathlib import Path


def sha256(path: Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def md5(path: Path) -> str:
    hasher = hashlib.md5()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", required=True, type=Path)
    args = parser.parse_args()
    repo = args.repo.resolve()
    debs = sorted(repo.glob("*.deb"))
    if not debs:
        raise SystemExit(f"No .deb packages found in local repository: {repo}")

    records: list[str] = []
    seen: set[tuple[str, str, str]] = set()
    for deb in debs:
        control = subprocess.check_output(
            ["dpkg-deb", "-f", str(deb)], text=True, encoding="utf-8"
        ).strip()
        fields = {}
        for line in control.splitlines():
            if line and not line[0].isspace() and ":" in line:
                key, value = line.split(":", 1)
                fields[key.lower()] = value.strip()
        identity = (
            fields.get("package", ""),
            fields.get("version", ""),
            fields.get("architecture", ""),
        )
        if not all(identity):
            raise SystemExit(f"Missing package identity in {deb}")
        if identity in seen:
            raise SystemExit(f"Duplicate package identity in local repository: {identity}")
        seen.add(identity)

        record = control
        record += f"\nFilename: {deb.name}"
        record += f"\nSize: {deb.stat().st_size}"
        record += f"\nMD5sum: {md5(deb)}"
        record += f"\nSHA256: {sha256(deb)}"
        records.append(record + "\n")

    content = "\n".join(records)
    (repo / "Packages").write_text(content, encoding="utf-8")
    with (repo / "Packages.gz").open("wb") as raw:
        with gzip.GzipFile(fileobj=raw, mode="wb", mtime=0) as compressed:
            compressed.write(content.encode("utf-8"))
    print(f"[PASS] Built local APT index for {len(records)} packages.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

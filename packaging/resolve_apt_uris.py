#!/usr/bin/env python3
"""Parse apt-get --print-uris output without installing packages."""
from __future__ import annotations

import argparse
import json
import shlex
from pathlib import Path

MIRROR_PREFIX = "mirror+file:/etc/apt/apt-mirrors.txt"
SNAPSHOT_PREFIX = "https://snapshot.ubuntu.com/ubuntu/"


def read_mirrors(path: Path) -> list[str]:
    if not path.is_file():
        raise SystemExit(f"APT mirror list is missing: {path}")
    mirrors: list[str] = []
    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        fields = raw_line.strip().split()
        if not fields or fields[0].startswith("#"):
            continue
        candidate = fields[0].rstrip("/")
        if candidate.startswith(("http://", "https://")):
            mirrors.append(candidate)
    if not mirrors:
        raise SystemExit(f"No HTTP(S) mirrors found in {path}")
    return mirrors


def expand_uri(uri: str, mirror_file: Path) -> list[str]:
    if uri.startswith(MIRROR_PREFIX):
        suffix = uri[len(MIRROR_PREFIX):].lstrip("/")
        if not suffix:
            raise SystemExit(f"APT mirror URI has no package path: {uri}")
        return [f"{base}/{suffix}" for base in read_mirrors(mirror_file)]
    if uri.startswith(SNAPSHOT_PREFIX):
        remainder = uri[len(SNAPSHOT_PREFIX):]
        snapshot_id, separator, suffix = remainder.partition("/")
        if not separator or not snapshot_id or not suffix:
            raise SystemExit(f"Malformed Ubuntu snapshot URI: {uri}")
        mirrors = [uri]
        try:
            mirrors.extend(f"{base}/{suffix}" for base in read_mirrors(mirror_file))
        except SystemExit:
            pass
        return list(dict.fromkeys(mirrors))
    if uri.startswith(("http://", "https://")):
        return [uri]
    raise SystemExit(f"Unsupported APT download URI scheme: {uri}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--mirror-file", type=Path, default=Path("/etc/apt/apt-mirrors.txt"))
    args = parser.parse_args()

    entries: dict[str, dict[str, object]] = {}
    for line_number, raw_line in enumerate(
        args.input.read_text(encoding="utf-8", errors="replace").splitlines(), 1
    ):
        line = raw_line.strip()
        if not line.startswith("'"):
            continue
        try:
            fields = shlex.split(line)
        except ValueError:
            continue
        if len(fields) < 3:
            continue
        uri, filename, size_text = fields[:3]
        if not filename.endswith(".deb"):
            continue
        try:
            size = int(size_text)
        except ValueError:
            continue
        mirrors = expand_uri(uri, args.mirror_file)
        checksums = {}
        for field in fields[3:]:
            if field.startswith("SHA256:"):
                checksums["sha256"] = field.partition(":")[2]
            elif field.startswith("MD5Sum:"):
                checksums["md5"] = field.partition(":")[2]
        entry = {
            "uri": uri,
            "mirrors": mirrors,
            "filename": filename,
            "size": size,
            "sha256": checksums.get("sha256", ""),
            "md5": checksums.get("md5", ""),
            "source_line": line_number,
        }
        previous = entries.get(filename)
        if previous and previous["mirrors"] != mirrors:
            raise SystemExit(f"APT returned conflicting URIs for {filename}")
        entries[filename] = entry

    if not entries:
        raise SystemExit(
            "No .deb download URIs were found. Check Ubuntu 24.04 APT package indexes; "
            "this parser does not run apt-get update or install."
        )

    result = sorted(entries.values(), key=lambda item: str(item["filename"]))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"[PASS] Parsed {len(result)} unique .deb download URIs.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

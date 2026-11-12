#!/usr/bin/env python3
"""Parse apt-get --print-uris output without installing packages."""
from __future__ import annotations

import argparse
import json
import shlex
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
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
        if not uri.startswith(("http://", "https://")) or not filename.endswith(".deb"):
            continue
        try:
            size = int(size_text)
        except ValueError:
            continue
        digest = ""
        for field in fields[3:]:
            if field.startswith("SHA256:"):
                digest = field.partition(":")[2]
                break
        entry = {
            "uri": uri,
            "filename": filename,
            "size": size,
            "sha256": digest,
            "source_line": line_number,
        }
        previous = entries.get(filename)
        if previous and previous["uri"] != uri:
            raise SystemExit(f"APT returned conflicting URIs for {filename}")
        entries[filename] = entry

    if not entries:
        raise SystemExit(
            "No .deb download URIs were found. Check that Ubuntu 24.04 apt package indexes "
            "are available; this tool intentionally does not run apt-get update or install."
        )

    result = sorted(entries.values(), key=lambda item: str(item["filename"]))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(f"[PASS] Parsed {len(result)} unique .deb download URIs.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

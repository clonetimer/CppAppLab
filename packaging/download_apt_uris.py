#!/usr/bin/env python3
"""Download APT-resolved .deb files with mirror fallback and integrity checks."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import tempfile
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path


def digest(path: Path, algorithm: str) -> str:
    hasher = hashlib.new(algorithm)
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def download_one(entry: dict[str, object], output: Path) -> None:
    filename = str(entry["filename"])
    if Path(filename).name != filename or filename in ("", ".", ".."):
        raise RuntimeError(f"Unsafe package filename: {filename}")
    expected_size = int(entry["size"])
    expected_sha = str(entry.get("sha256") or "").lower()
    expected_md5 = str(entry.get("md5") or "").lower()
    target = output / filename

    def valid(path: Path) -> bool:
        if not path.is_file() or path.stat().st_size != expected_size:
            return False
        if expected_sha and digest(path, "sha256") != expected_sha:
            return False
        if expected_md5 and digest(path, "md5") != expected_md5:
            return False
        return True

    if valid(target):
        print(f"[SKIP] Verified existing {filename}", flush=True)
        return
    target.unlink(missing_ok=True)

    mirrors = [str(uri) for uri in entry.get("mirrors", [])]
    if not mirrors and entry.get("uri"):
        mirrors = [str(entry["uri"])]
    errors: list[str] = []
    for uri in mirrors:
        fd, temp_name = tempfile.mkstemp(prefix=f".{filename}.", suffix=".part", dir=output)
        os.close(fd)
        temp_path = Path(temp_name)
        try:
            print(f"[INFO] Downloading {filename} from {uri}", flush=True)
            request = urllib.request.Request(
                uri, headers={"User-Agent": "CppAppLab-offline-packager/1"}
            )
            with urllib.request.urlopen(request, timeout=90) as response, temp_path.open("wb") as dest:
                while True:
                    chunk = response.read(1024 * 1024)
                    if not chunk:
                        break
                    dest.write(chunk)
            if temp_path.stat().st_size != expected_size:
                raise RuntimeError(
                    f"size mismatch: expected {expected_size}, got {temp_path.stat().st_size}"
                )
            if expected_sha and digest(temp_path, "sha256") != expected_sha:
                raise RuntimeError("SHA-256 mismatch")
            if expected_md5 and digest(temp_path, "md5") != expected_md5:
                raise RuntimeError("MD5 mismatch against signed APT package index")
            temp_path.replace(target)
            print(f"[PASS] {filename} ({expected_size} bytes)", flush=True)
            return
        except Exception as exc:
            errors.append(f"{uri}: {exc}")
            temp_path.unlink(missing_ok=True)

    raise RuntimeError(
        f"All configured APT mirrors failed for {filename}:\n" + "\n".join(errors)
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--workers", type=int, default=8)
    args = parser.parse_args()
    if not 1 <= args.workers <= 32:
        raise SystemExit("--workers must be between 1 and 32")
    args.output.mkdir(parents=True, exist_ok=True)
    entries = json.loads(args.input.read_text(encoding="utf-8"))
    if not isinstance(entries, list) or not entries:
        raise SystemExit("APT URI manifest is empty or invalid")
    with ThreadPoolExecutor(max_workers=args.workers) as executor:
        futures = [executor.submit(download_one, entry, args.output) for entry in entries]
        for future in as_completed(futures):
            future.result()
    print(f"[PASS] Downloaded and verified {len(entries)} package archives.", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        raise SystemExit(f"[FAIL] {exc}")

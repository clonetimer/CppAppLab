"""Replay reviewed UCRT64 archives on a clean MSYS2 CI runner.

Deliberately not wired into packaging until the D2a hash report is reviewed
and its exact bytes/identity frozen. No fallback to pacman -S or current repo.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
from urllib.parse import quote
from urllib.request import urlopen

from package_windows_native import digest
from verify_windows_toolchain_lock import installed_versions, require_exact, PREFIX, REPOSITORY_URL

SHA256 = re.compile(r"[0-9a-f]{64}")
ARCHIVE = re.compile(r"[A-Za-z0-9_.+~-]+\.pkg\.tar\.(?:zst|xz)")
CONFIG = """[options]
Architecture = auto
SigLevel = Required TrustedOnly
LocalFileSigLevel = Required TrustedOnly
RemoteFileSigLevel = Required TrustedOnly
CheckSpace
"""
# No repository sections in this config: the local transaction cannot resolve
# a missing dependency by downloading whatever is currently available.


def load_frozen_lock(baseline_path: Path, lock_path: Path) -> dict:
    baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
    lock = json.loads(lock_path.read_text(encoding="utf-8"))
    if (baseline.get("schema_version") != 1 or
            baseline.get("status") != "OBSERVED_VERSION_LOCK" or
            lock.get("schema_version") != 1 or
            lock.get("status") != "FROZEN_ARCHIVE_LOCK" or
            lock.get("version_baseline_sha256") != digest(baseline_path) or
            lock.get("baseline") != baseline.get("baseline")):
        raise RuntimeError("A reviewed, frozen archive lock matching the version baseline is required.")
    review = lock.get("review", {})
    if (type(review.get("run_id")) is not int or review["run_id"] <= 0 or
            type(review.get("artifact_id")) is not int or review["artifact_id"] <= 0 or
            not isinstance(review.get("report_sha256"), str) or
            not SHA256.fullmatch(review["report_sha256"])):
        raise RuntimeError("Frozen lock lacks acquisition report provenance.")
    expected = baseline["packages"]
    records = lock["packages"]
    if not expected or any(not name.startswith(PREFIX) for name in expected):
        raise RuntimeError("Invalid UCRT64 baseline package set.")
    require_exact(expected, {name: entry["version"] for name, entry in records.items()})
    filenames = set()
    for name, record in records.items():
        filename = record.get("filename", "")
        sha = record.get("sha256", "")
        if (not isinstance(filename, str) or not ARCHIVE.fullmatch(filename) or
                not filename.startswith(name + "-") or filename.casefold() in filenames or
                record.get("repository") != "ucrt64" or
                record.get("url") != REPOSITORY_URL + quote(filename) or
                not isinstance(sha, str) or not SHA256.fullmatch(sha) or
                type(record.get("bytes")) is not int or record["bytes"] <= 0):
            raise RuntimeError(f"Invalid frozen archive record: {name}")
        filenames.add(filename.casefold())
    return lock


def verify_archive(path: Path, record: dict) -> None:
    if not path.is_file() or path.stat().st_size != record["bytes"]:
        raise RuntimeError(f"Frozen archive size mismatch: {path.name}")
    if digest(path) != record["sha256"]:
        raise RuntimeError(f"Frozen archive SHA-256 mismatch: {path.name}")


def download(url: str, destination: Path, maximum: int) -> None:
    partial = destination.with_name(destination.name + ".part")
    count = 0
    try:
        with urlopen(url, timeout=60) as response, partial.open("xb") as target:
            # Only the exact approved upstream URL, no mirror/redirect fallback.
            if response.geturl() != url:
                raise RuntimeError(f"Unexpected upstream redirect: {response.geturl()}")
            while True:
                chunk = response.read(1024 * 1024)
                if not chunk:
                    break
                count += len(chunk)
                if count > maximum:
                    raise RuntimeError(f"Download exceeds locked size limit: {destination.name}")
                target.write(chunk)
        if count == 0:
            raise RuntimeError(f"Empty upstream download: {destination.name}")
        partial.rename(destination)
    finally:
        partial.unlink(missing_ok=True)


def replay(msys: Path, baseline: Path, lock_path: Path, output: Path) -> None:
    if os.name != "nt" or os.environ.get("GITHUB_ACTIONS") != "true":
        raise RuntimeError("Replay is restricted to a Windows GitHub Actions acquisition runner.")
    lock = load_frozen_lock(baseline, lock_path)
    records = lock["packages"]
    if installed_versions(msys):
        raise RuntimeError("Start with a clean runner with no preinstalled UCRT64 packages.")
    pacman = msys / "usr/bin/pacman.exe"
    cygpath = msys / "usr/bin/cygpath.exe"
    env = dict(os.environ, PATH=str(pacman.parent) + os.pathsep + os.environ["PATH"],
               MSYSTEM="MSYS", LC_ALL="C")

    def posix(path: Path) -> str:
        return subprocess.check_output([str(cygpath), "-u", str(path.resolve())],
                                       env=env, encoding="utf-8", timeout=15).strip()

    with tempfile.TemporaryDirectory(prefix="cppapplab-frozen-acquisition-") as folder:
        scratch = Path(folder)
        archives = []
        for name, record in sorted(records.items()):
            archive = scratch / record["filename"]
            download(record["url"], archive, record["bytes"])
            verify_archive(archive, record)
            signature = archive.with_name(archive.name + ".sig")
            download(record["url"] + ".sig", signature, 1024 * 1024)
            archives.append(archive)
            print(f"[PASS] Verified locked archive bytes: {name}", flush=True)
        config = scratch / "pacman-local-only.conf"
        config.write_text(CONFIG, encoding="ascii", newline="\n")
        # Single transaction: all locked archives are explicit arguments.
        # Pacman verifies detached signatures using the runner's trusted keyring;
        # no unsigned-local exception, repository, dependency skip or overwrite.
        subprocess.run([str(pacman), "--config", posix(config), "--noconfirm",
                        "--noprogressbar", "--color", "never", "-U",
                        *[posix(path) for path in archives]],
                       env=env, check=True, timeout=900)
        require_exact({name: record["version"] for name, record in records.items()},
                      installed_versions(msys))
    report = {"schema_version": 1, "status": "FROZEN_ARCHIVE_REPLAY_VERIFIED",
              "version_baseline_sha256": digest(baseline),
              "frozen_lock_sha256": digest(lock_path), "baseline": lock["baseline"],
              "review": lock["review"], "packages": records,
              "signature_policy": "Required TrustedOnly; no configured repositories",
              "limitations": ["Runner installation verified; full ZIP acceptance separate.",
                              "Corresponding-source/redistribution review remains separate."]}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"[PASS] Replayed {len(records)} exact signed UCRT64 archives.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--msys2-root", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--lock", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    replay(args.msys2_root, args.baseline, args.lock, args.output)

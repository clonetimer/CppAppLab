#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PARSER = ROOT / "packaging" / "resolve_apt_uris.py"
DOWNLOADER = ROOT / "packaging" / "download_apt_uris.py"


class AptUriParserTests(unittest.TestCase):
    def run_parser(
        self, text: str, mirror_text: str | None = None
    ) -> tuple[subprocess.CompletedProcess[str], Path]:
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        root = Path(temp.name)
        source = root / "apt-output.txt"
        target = root / "uris.json"
        mirror_file = root / "apt-mirrors.txt"
        source.write_text(text, encoding="utf-8")
        args = [sys.executable, str(PARSER), "--input", str(source), "--output", str(target)]
        if mirror_text is not None:
            mirror_file.write_text(mirror_text, encoding="utf-8")
            args.extend(["--mirror-file", str(mirror_file)])
        result = subprocess.run(args, capture_output=True, text=True, check=False)
        return result, target

    def test_parses_download_uris_and_ignores_non_uri_lines(self) -> None:
        output = """
Reading package lists...
The following NEW packages will be installed:
  libexample1
'http://archive.ubuntu.com/ubuntu/pool/main/e/example/libexample1_1.2.3_amd64.deb' libexample1_1.2.3_amd64.deb 12345 SHA256:abcdef
'https://security.ubuntu.com/ubuntu/pool/main/e/example/libexample2_2.0_all.deb' libexample2_2.0_all.deb 67890 SHA256:012345
Need to get 80.0 kB of archives.
"""
        result, target = self.run_parser(output)
        self.assertEqual(result.returncode, 0, result.stderr)
        data = json.loads(target.read_text(encoding="utf-8"))
        self.assertEqual(len(data), 2)
        self.assertEqual(data[0]["filename"], "libexample1_1.2.3_amd64.deb")
        self.assertEqual(data[0]["size"], 12345)
        self.assertEqual(data[0]["sha256"], "abcdef")
        self.assertEqual(data[1]["filename"], "libexample2_2.0_all.deb")

    def test_expands_mirror_file_uri_in_priority_order(self) -> None:
        output = (
            "'mirror+file:/etc/apt/apt-mirrors.txt/pool/main/e/example/pkg.deb' "
            "pkg.deb 100 MD5Sum:abc\n"
        )
        mirrors = (
            "https://mirror-one.example/ubuntu/ priority:1\n"
            "https://mirror-two.example/ubuntu/ priority:2\n"
        )
        result, target = self.run_parser(output, mirrors)
        self.assertEqual(result.returncode, 0, result.stderr)
        data = json.loads(target.read_text(encoding="utf-8"))
        self.assertEqual(
            data[0]["mirrors"],
            [
                "https://mirror-one.example/ubuntu/pool/main/e/example/pkg.deb",
                "https://mirror-two.example/ubuntu/pool/main/e/example/pkg.deb",
            ],
        )
        self.assertEqual(data[0]["md5"], "abc")

    def test_snapshot_uri_prefers_snapshot_then_configured_mirrors(self) -> None:
        output = (
            "'https://snapshot.ubuntu.com/ubuntu/20261112T000000Z/pool/main/e/example/pkg.deb' "
            "pkg.deb 100 MD5Sum:abc\n"
        )
        mirrors = (
            "https://mirror-one.example/ubuntu/ priority:1\n"
            "https://mirror-two.example/ubuntu/ priority:2\n"
        )
        result, target = self.run_parser(output, mirrors)
        self.assertEqual(result.returncode, 0, result.stderr)
        data = json.loads(target.read_text(encoding="utf-8"))
        self.assertEqual(
            data[0]["mirrors"],
            [
                "https://snapshot.ubuntu.com/ubuntu/20261112T000000Z/pool/main/e/example/pkg.deb",
                "https://mirror-one.example/ubuntu/pool/main/e/example/pkg.deb",
                "https://mirror-two.example/ubuntu/pool/main/e/example/pkg.deb",
            ],
        )

    def test_deduplicates_identical_uri_rows(self) -> None:
        line = "'https://example.invalid/pkg.deb' pkg.deb 100 SHA256:abc\n"
        result, target = self.run_parser(line + line)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(len(json.loads(target.read_text(encoding="utf-8"))), 1)

    def test_rejects_conflicting_uris_for_same_filename(self) -> None:
        output = (
            "'https://one.invalid/pkg.deb' pkg.deb 100 SHA256:abc\n"
            "'https://two.invalid/pkg.deb' pkg.deb 100 SHA256:abc\n"
        )
        result, _ = self.run_parser(output)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("conflicting URIs", result.stderr)

    def test_fails_when_no_deb_uri_is_present(self) -> None:
        result, _ = self.run_parser("Reading package lists...\nDone\n")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("No .deb download URIs", result.stderr)

    def test_downloader_uses_second_mirror_and_checks_digest(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "source.deb"
            payload = b"test-deb-payload"
            source.write_bytes(payload)
            sha = hashlib.sha256(payload).hexdigest()
            md5 = hashlib.md5(payload).hexdigest()
            manifest = root / "uris.json"
            manifest.write_text(json.dumps([{
                "filename": "pkg.deb",
                "size": len(payload),
                "sha256": sha,
                "md5": md5,
                "mirrors": [
                    (root / "missing.deb").as_uri(),
                    source.as_uri(),
                ],
            }]), encoding="utf-8")
            output = root / "downloads"
            result = subprocess.run(
                [sys.executable, str(DOWNLOADER), "--input", str(manifest), "--output", str(output)],
                capture_output=True, text=True, check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((output / "pkg.deb").read_bytes(), payload)

    def test_downloader_rejects_checksum_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "source.deb"
            source.write_bytes(b"not-the-expected-package")
            manifest = root / "uris.json"
            manifest.write_text(json.dumps([{
                "filename": "pkg.deb",
                "size": source.stat().st_size,
                "sha256": "0" * 64,
                "mirrors": [source.as_uri()],
            }]), encoding="utf-8")
            result = subprocess.run(
                [sys.executable, str(DOWNLOADER), "--input", str(manifest), "--output", str(root / "out")],
                capture_output=True, text=True, check=False,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("SHA-256 mismatch", result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)

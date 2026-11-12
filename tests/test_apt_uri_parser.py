#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PARSER = ROOT / "packaging" / "resolve_apt_uris.py"


class AptUriParserTests(unittest.TestCase):
    def run_parser(self, text: str) -> tuple[subprocess.CompletedProcess[str], Path]:
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        root = Path(temp.name)
        source = root / "apt-output.txt"
        target = root / "uris.json"
        source.write_text(text, encoding="utf-8")
        result = subprocess.run(
            [sys.executable, str(PARSER), "--input", str(source), "--output", str(target)],
            capture_output=True,
            text=True,
            check=False,
        )
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


if __name__ == "__main__":
    unittest.main(verbosity=2)

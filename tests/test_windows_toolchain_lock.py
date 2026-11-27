"""Portable tests for D2a lock rejection; actual capture runs only on Windows."""
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from verify_windows_toolchain_lock import parse_plan, require_exact, verify_archives

NAME = "mingw-w64-ucrt-x86_64-example"
VERSION = "1.2-3"
FILENAME = NAME + "-" + VERSION + "-any.pkg.tar.zst"
PAYLOAD = b"fixture-only archive bytes"
SHA = hashlib.sha256(PAYLOAD).hexdigest()
LINE = f"{NAME}|{VERSION}|ucrt64|{FILENAME}|{SHA}"


class TestLock(unittest.TestCase):
    def test_exact_version(self):
        require_exact({NAME: VERSION}, {NAME: VERSION})

    def test_missing_extra_changed_are_rejected(self):
        for actual in ({}, {NAME: VERSION, "extra": "2"}, {NAME: "1.2-4"}):
            with self.subTest(actual=actual), self.assertRaises(RuntimeError):
                require_exact({NAME: VERSION}, actual)

    def test_plan_and_bytes(self):
        records = parse_plan(LINE, {NAME: VERSION})
        with tempfile.TemporaryDirectory() as d:
            cache = Path(d)
            (cache / FILENAME).write_bytes(PAYLOAD)
            verify_archives(records, cache)
            self.assertEqual(records[NAME]["bytes"], len(PAYLOAD))
            (cache / FILENAME).write_bytes(b"wrong")
            with self.assertRaisesRegex(RuntimeError, "SHA-256 mismatch"):
                verify_archives(records, cache)
            (cache / FILENAME).unlink()
            with self.assertRaisesRegex(RuntimeError, "archive missing"):
                verify_archives(records, cache)

    def test_bad_plan_rejected(self):
        for output in (LINE + "\n" + LINE, LINE.replace("ucrt64|", "mingw64|"),
                       LINE.replace(FILENAME, "../" + FILENAME),
                       LINE.replace(SHA, "invalid"), LINE.replace(VERSION, "1.2-4")):
            with self.subTest(output=output), self.assertRaises(RuntimeError):
                parse_plan(output, {NAME: VERSION})


if __name__ == "__main__":
    unittest.main()

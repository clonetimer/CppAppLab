"""Pure fixture tests; do not claim Windows installation or signature PASS."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from replay_windows_toolchain_lock import CONFIG, load_frozen_lock, verify_archive
from verify_windows_toolchain_lock import REPOSITORY_URL

NAME = "mingw-w64-ucrt-x86_64-fixture"
VERSION = "1.0-1"
FILE = NAME + "-" + VERSION + "-any.pkg.tar.zst"
BYTES = b"test-only archive payload"


class TestReplay(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.baseline = self.root / "baseline.json"
        self.lock = self.root / "frozen.json"
        base = {"schema_version": 1, "status": "OBSERVED_VERSION_LOCK",
                "baseline": {"run_id": 1}, "packages": {NAME: VERSION}}
        self.baseline.write_text(json.dumps(base), encoding="utf-8")
        self.record = {"version": VERSION, "repository": "ucrt64", "filename": FILE,
                       "sha256": hashlib.sha256(BYTES).hexdigest(), "bytes": len(BYTES),
                       "url": REPOSITORY_URL + FILE}
        self.data = {"schema_version": 1, "status": "FROZEN_ARCHIVE_LOCK",
                     "version_baseline_sha256": hashlib.sha256(self.baseline.read_bytes()).hexdigest(),
                     "baseline": base["baseline"],
                     "review": {"run_id": 1, "artifact_id": 2,
                                "report_sha256": hashlib.sha256(b"fixture").hexdigest()},
                     "packages": {NAME: self.record}}
        self.write()

    def write(self):
        self.lock.write_text(json.dumps(self.data), encoding="utf-8")

    def load(self):
        return load_frozen_lock(self.baseline, self.lock)

    def test_valid_lock_and_archive(self):
        self.assertEqual(self.load()["packages"][NAME], self.record)
        archive = self.root / FILE
        archive.write_bytes(BYTES)
        verify_archive(archive, self.record)
        archive.write_bytes(b"x" * len(BYTES))
        with self.assertRaisesRegex(RuntimeError, "SHA-256 mismatch"):
            verify_archive(archive, self.record)
        archive.write_bytes(b"")
        with self.assertRaisesRegex(RuntimeError, "size mismatch"):
            verify_archive(archive, self.record)

    def test_capture_is_not_frozen_lock(self):
        self.data["status"] = "ARCHIVE_HASH_CAPTURE_CANDIDATE"
        self.write()
        with self.assertRaisesRegex(RuntimeError, "frozen archive lock"):
            self.load()

    def test_missing_review(self):
        self.data["review"] = {}
        self.write()
        with self.assertRaisesRegex(RuntimeError, "provenance"):
            self.load()

    def test_version_drift(self):
        self.record["version"] = "1.0-2"
        self.write()
        with self.assertRaisesRegex(RuntimeError, "version drift"):
            self.load()

    def test_malformed_records(self):
        for key, value in (("filename", "../" + FILE), ("url", "https://example.com/" + FILE),
                           ("repository", "mingw64"), ("sha256", "x"), ("bytes", 0)):
            old = self.record[key]
            with self.subTest(key=key):
                self.record[key] = value
                self.write()
                with self.assertRaisesRegex(RuntimeError, "Invalid frozen archive"):
                    self.load()
            self.record[key] = old

    def test_config_requires_trusted_signatures_and_has_no_repos(self):
        self.assertIn("LocalFileSigLevel = Required TrustedOnly", CONFIG)
        self.assertEqual([line for line in CONFIG.splitlines() if line.startswith("[")],
                         ["[options]"])


if __name__ == "__main__":
    unittest.main()

"""Freeze the actual engine status spelling and raw snapshot fingerprint."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import source_transfer as transfer


class SourceStatusTests(unittest.TestCase):
    def test_actual_status_format_and_windows_line_endings(self):
        status = ("Workspace: C:/Learner Space/w1.hello-world\r\n"
                  "Last recorded result: PASS\r\nAcceptance: 1/1\r\n"
                  "Source fingerprint: fnv1a64:0123456789abcdef\r\n")
        self.assertEqual(transfer.current_pass_fingerprint(status, 1),
                         "fnv1a64:0123456789abcdef")

    def test_reject_wrong_count_stale_bare_hex_and_duplicate(self):
        good = ("Last recorded result: PASS\nAcceptance: 9/9\n"
                "Source fingerprint: fnv1a64:0123456789abcdef\n")
        for bad in (good.replace("9/9", "8/9"), good.replace("PASS", "STALE"),
                    good.replace("fnv1a64:", ""), good + "Last recorded result: FAIL\n",
                    good + "Source fingerprint: fnv1a64:0123456789abcdef\n"):
            with self.subTest(bad=bad), self.assertRaises(transfer.TransferError):
                transfer.current_pass_fingerprint(bad, 9)

    def test_snapshot_path_order_and_raw_bytes(self):
        sources = {"src/main.cpp": b"int main(){}\r\n", "include/x.hpp": b"// x\n"}
        fingerprint = transfer.snapshot_fingerprint(sources)
        self.assertRegex(fingerprint, r"^fnv1a64:[0-9a-f]{16}$")
        self.assertNotEqual(fingerprint, transfer.snapshot_fingerprint(dict(reversed(list(sources.items())))))
        self.assertNotEqual(fingerprint, transfer.snapshot_fingerprint(dict(sources, **{"src/main.cpp": b"int main(){}\n"})))


if __name__ == "__main__":
    unittest.main()

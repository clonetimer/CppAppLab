import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import wsl_handoff as wsl


class TestHandoff(unittest.TestCase):
    def setUp(self):
        self.catalog = "a" * 64
        self.instance = "b" * 32
        self.calls = []
        self.receipt = {"task": "w5.system-lab", "task_state": "LOCKED",
                        "execution_environment": "linux", "catalog_sha256": self.catalog,
                        "instance_id": self.instance}

    def run(self, command, timeout):
        self.calls.append((command, timeout))
        if command[1:] == ["--list", "--verbose"]:
            return "* Ubuntu-22.04    Stopped    2\n  Ubuntu-24.04    Running    2\n".encode("utf-16-le")
        return (wsl.MARKER + json.dumps(self.receipt) + "\n").encode()

    def health(self, port):
        return {"status": "ok", "service": "CppAppLab", "execution_environment": "linux",
                "instance_id": self.instance, "catalog_sha256": self.catalog}

    def handoff(self, task="w5.system-lab", port=8765):
        return wsl.handoff(task, port, self.catalog, run=self.run,
                           health=self.health, wsl=Path("wsl.exe"))

    def test_supported_default_and_no_progress_transfer(self):
        result = self.handoff()
        self.assertEqual(result["distribution"], "Ubuntu-22.04")
        self.assertEqual(result["url"], "http://127.0.0.1:8766/#task=w5.system-lab")
        self.assertEqual(result["task_state"], "LOCKED")
        self.assertFalse(result["progress_transferred"])
        self.assertEqual(self.calls[1][0][-2:], ["w5.system-lab", "8766"])
        self.assertEqual(len(self.calls), 2)

    def test_alternate_port_and_supported_priority(self):
        self.assertEqual(self.handoff(port=8766)["url"], "http://127.0.0.1:8767/#task=w5.system-lab")
        self.assertEqual(wsl.select_distro("* Debian Running 2\n Ubuntu-22.04 Stopped 2\n Ubuntu-24.04 Running 2"), "Ubuntu-24.04")

    def test_wsl1_and_unsupported_rejected(self):
        with self.assertRaises(wsl.HandoffError):
            wsl.select_distro("* Ubuntu-24.04 Running 1\n Debian Running 2")

    def test_invalid_task_never_runs_wsl(self):
        with self.assertRaises(wsl.HandoffError):
            self.handoff("w5.system-lab;whoami")
        self.assertEqual(self.calls, [])

    def test_wrong_catalog_or_instance_never_redirects(self):
        self.receipt["catalog_sha256"] = "c" * 64
        with self.assertRaises(wsl.HandoffError):
            self.handoff()
        self.receipt["catalog_sha256"] = self.catalog
        self.receipt["instance_id"] = "d" * 32
        with self.assertRaises(wsl.HandoffError):
            self.handoff()

    def test_native_endpoint_never_redirects(self):
        self.health = lambda port: {"status": "ok", "service": "CppAppLab",
                                    "execution_environment": "native-windows",
                                    "instance_id": self.instance, "catalog_sha256": self.catalog}
        with self.assertRaises(wsl.HandoffError):
            self.handoff()


if __name__ == "__main__":
    unittest.main()

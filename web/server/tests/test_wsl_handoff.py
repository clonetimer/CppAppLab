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
                        "instance_id": self.instance,
                        "next_task": "w1.hello-world",
                        "pending_prerequisites": [{"task_id": "w1.hello-world", "state": "AVAILABLE"},
                                                  {"task_id": "w1.calculator", "state": "LOCKED"}]}

    def fake_run(self, command, timeout):
        self.calls.append((command, timeout))
        if command[1:] == ["--list", "--verbose"]:
            return "* Ubuntu-22.04    Stopped    2\n  Ubuntu-24.04    Running    2\n".encode("utf-16-le")
        return (wsl.MARKER + json.dumps(self.receipt) + "\n").encode()

    def health(self, port):
        return {"status": "ok", "service": "CppAppLab", "execution_environment": "linux",
                "instance_id": self.instance, "catalog_sha256": self.catalog}

    def handoff(self, task="w5.system-lab", port=8765):
        return wsl.handoff(task, port, self.catalog, run=self.fake_run,
                           health=self.health, wsl=Path("wsl.exe"))

    def test_supported_default_and_no_progress_transfer(self):
        result = self.handoff()
        self.assertEqual(result["distribution"], "Ubuntu-22.04")
        self.assertEqual(result["url"], "http://127.0.0.1:8766/#task=w1.hello-world&goal=w5.system-lab")
        self.assertEqual(result["next_task_id"], "w1.hello-world")
        self.assertEqual(len(result["pending_prerequisites"]), 2)
        self.assertEqual(result["task_state"], "LOCKED")
        self.assertFalse(result["progress_transferred"])
        self.assertEqual(self.calls[1][0][-2:], ["w5.system-lab", "8766"])
        self.assertEqual(len(self.calls), 2)

    def test_alternate_port_and_supported_priority(self):
        self.receipt.update(task_state="AVAILABLE", next_task="w5.system-lab", pending_prerequisites=[])
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

    def test_real_prerequisite_order_and_stale_evidence(self):
        tasks = {"w1.hello-world": {"prerequisites": []},
                 "w1.calculator": {"prerequisites": ["w1.hello-world"]},
                 "w5.system-lab": {"prerequisites": ["w1.calculator"]}}
        states = {"w1.hello-world": "PASSED", "w1.calculator": "STALE", "w5.system-lab": "LOCKED"}
        plan = wsl.prerequisite_plan(tasks, states, "w5.system-lab")
        self.assertEqual(plan["next_task"], "w1.calculator")
        states["w1.calculator"] = "PASSED"
        states["w5.system-lab"] = "AVAILABLE"
        self.assertEqual(wsl.prerequisite_plan(tasks, states, "w5.system-lab")["next_task"], "w5.system-lab")
        states["w1.hello-world"] = "UNKNOWN"
        with self.assertRaises(wsl.HandoffError):
            wsl.prerequisite_plan(tasks, states, "w5.system-lab")

    def test_inconsistent_or_injected_prerequisite_never_redirects(self):
        self.receipt["next_task"] = "w1.calculator"
        with self.assertRaises(wsl.HandoffError):
            self.handoff()
        self.receipt["next_task"] = "w1.hello-world"
        self.receipt["pending_prerequisites"][0]["state"] = "LOCKED"
        with self.assertRaises(wsl.HandoffError):
            self.handoff()
        self.receipt["pending_prerequisites"][0] = {"task_id": "w1.hello-world;whoami", "state": "AVAILABLE"}
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

from __future__ import annotations

import json
import os
import subprocess
import tempfile
import threading
import urllib.error
import urllib.request
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "web" / "server"))
from cppapplab_web import CppAppLabWebServer


class RealEngineBrowserAcceptance(unittest.TestCase):
    def setUp(self) -> None:
        binary_value = os.environ.get("CPPAPPLAB_TEST_BINARY")
        if not binary_value:
            self.skipTest("CPPAPPLAB_TEST_BINARY is not set; this is a post-build integration test.")
        self.temporary = tempfile.TemporaryDirectory(prefix="cppapplab-web-e2e-")
        self.base = Path(self.temporary.name)
        self.home = self.base / "home"
        self.home.mkdir()
        self.workspace_root = self.home / "cppapplab-work"
        self.workspace_root.mkdir()
        self.state_root = self.home / ".local" / "share" / "cppapplab"
        self.state_root.mkdir(parents=True)
        self.server = CppAppLabWebServer(
            ("127.0.0.1", 0),
            ROOT,
            Path(binary_value),
            self.workspace_root,
            ROOT / "web" / "ui",
        )
        self.server.child_env["HOME"] = str(self.home)
        self.server.child_env["CPPAPPLAB_STATE_DIR"] = str(self.state_root)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.base_url = f"http://127.0.0.1:{self.server.server_port}"
        self.token = self.request("/api/v1/session")["csrf_token"]

    def tearDown(self) -> None:
        if hasattr(self, "server"):
            self.server.shutdown()
            self.server.server_close()
            self.thread.join(timeout=3)
        if hasattr(self, "temporary"):
            self.temporary.cleanup()

    def request(self, path: str, method: str = "GET", body: dict | None = None):
        data = None if body is None else json.dumps(body).encode("utf-8")
        headers = {}
        if body is not None:
            headers["Content-Type"] = "application/json"
        if method in {"POST", "PUT"}:
            headers["Origin"] = self.base_url
            headers["X-CppAppLab-Token"] = self.token
        request = urllib.request.Request(
            self.base_url + path, data=data, headers=headers, method=method
        )
        with urllib.request.urlopen(request, timeout=200) as response:
            raw = response.read()
            if response.headers.get_content_type() == "application/json":
                return json.loads(raw.decode("utf-8"))
            return raw.decode("utf-8")

    def test_local_launcher_start_status_stop(self) -> None:
        runtime_dir = self.base / "runtime"
        state_dir = self.base / "state"
        env = os.environ.copy()
        env.update({
            "HOME": str(self.home),
            "XDG_RUNTIME_DIR": str(runtime_dir),
            "XDG_STATE_HOME": str(state_dir),
            "CPPAPPLAB_BINARY": os.environ["CPPAPPLAB_TEST_BINARY"],
            "CPPAPPLAB_WEB_PORT": "18765",
        })
        command = ["bash", str(ROOT / "scripts" / "start-web-workbench.sh")]
        try:
            started = subprocess.run(
                command + ["start"], env=env, capture_output=True, text=True,
                timeout=20, check=True,
            )
            self.assertIn("Web Workbench is ready", started.stdout)
            status = subprocess.run(
                command + ["status"], env=env, capture_output=True, text=True,
                timeout=10, check=True,
            )
            self.assertIn("is ready", status.stdout)
            stopped = subprocess.run(
                command + ["stop"], env=env, capture_output=True, text=True,
                timeout=10, check=True,
            )
            self.assertIn("Web Workbench stopped", stopped.stdout)
        finally:
            subprocess.run(
                command + ["stop"], env=env, capture_output=True, text=True,
                timeout=10, check=False,
            )

    def test_w1_start_edit_acceptance_and_stale_evidence(self) -> None:
        tasks = self.request("/api/v1/tasks")
        hello = next(task for task in tasks["tasks"] if task["id"] == "w1.hello-world")
        self.assertEqual(hello["state"], "AVAILABLE")

        started = self.request(
            "/api/v1/tasks/w1.hello-world/start", method="POST", body={}
        )
        self.assertTrue(started["workspace_ready"])
        self.assertFalse(started["resumed"])

        saved = self.request(
            "/api/v1/workspaces/w1.hello-world/files/src/main.cpp",
            method="PUT",
            body={
                "content": (
                    "#include <iostream>\n"
                    "int main() {\n"
                    '    std::cout << "Hello, CppAppLab!\\n";\n'
                    "    return 0;\n"
                    "}\n"
                )
            },
        )
        self.assertTrue(saved["saved"])

        result = self.request(
            "/api/v1/workspaces/w1.hello-world/check",
            method="POST",
            body={},
        )
        self.assertTrue(result["passed"], result["output"])
        self.assertIn("Hello, CppAppLab!", result["output"])

        passed_tasks = self.request("/api/v1/tasks")
        hello = next(task for task in passed_tasks["tasks"] if task["id"] == "w1.hello-world")
        self.assertEqual(hello["state"], "PASSED")

        self.request(
            "/api/v1/workspaces/w1.hello-world/files/src/main.cpp",
            method="PUT",
            body={"content": "#include <iostream>\nint main() { return 0; }\n"},
        )
        stale_tasks = self.request("/api/v1/tasks")
        hello = next(task for task in stale_tasks["tasks"] if task["id"] == "w1.hello-world")
        calculator = next(task for task in stale_tasks["tasks"] if task["id"] == "w1.calculator")
        self.assertEqual(hello["state"], "STALE")
        self.assertEqual(calculator["state"], "LOCKED")

        request = urllib.request.Request(
            self.base_url + "/api/v1/tasks/w1.calculator/start",
            data=b"{}",
            headers={
                "Content-Type": "application/json",
                "Origin": self.base_url,
                "X-CppAppLab-Token": self.token,
            },
            method="POST",
        )
        with self.assertRaises(urllib.error.HTTPError) as error:
            urllib.request.urlopen(request, timeout=10)
        self.assertEqual(error.exception.code, 409)


if __name__ == "__main__":
    unittest.main(verbosity=2)

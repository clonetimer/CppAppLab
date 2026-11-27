from __future__ import annotations

import json
import hashlib
import os
import subprocess
import socket
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

    def test_catalog_hints_match_cli_without_writing_progress(self) -> None:
        def progress_snapshot() -> dict[str, bytes]:
            return {
                str(path.relative_to(self.state_root)): path.read_bytes()
                for path in self.state_root.rglob("*") if path.is_file()
            }

        before = progress_snapshot()
        for task in self.server.tasks:
            # A disposable catalog marker is enough for read-only hint lookup.
            # This does not unlock, run or accept any learner task.
            workspace = self.base / "hint-workspaces" / task["id"]
            workspace.mkdir(parents=True)
            marker = workspace / ".cppapplab-workspace"
            marker_text = f"CppAppLab Workspace v1\ntask_id={task['id']}\n"
            marker.write_text(marker_text, encoding="utf-8")
            for level in (1, 2, 3):
                with self.subTest(task=task["id"], level=level):
                    cli = subprocess.run(
                        [os.environ["CPPAPPLAB_TEST_BINARY"], "hint", str(workspace), str(level)],
                        env=self.server.child_env, capture_output=True, text=True,
                        encoding="utf-8", timeout=10,
                    )
                    self.assertEqual(cli.returncode, 0, cli.stderr)
                    browser = self.request(f"/api/v1/tasks/{task['id']}/hints/{level}")
                    self.assertTrue(browser["markdown"].strip())
                    self.assertEqual(cli.stdout.strip(), browser["markdown"].strip())
            self.assertEqual(marker.read_text(encoding="utf-8"), marker_text)
            self.assertEqual(list(workspace.iterdir()), [marker])
        self.assertEqual(progress_snapshot(), before)

    def test_local_launcher_start_status_stop(self) -> None:
        runtime_dir = self.base / "runtime"
        state_dir = self.base / "state"
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            port = reservation.getsockname()[1]
        env = os.environ.copy()
        env.update({
            "HOME": str(self.home),
            "XDG_RUNTIME_DIR": str(runtime_dir),
            "XDG_STATE_HOME": str(state_dir),
            "CPPAPPLAB_BINARY": os.environ["CPPAPPLAB_TEST_BINARY"],
            "CPPAPPLAB_WEB_PORT": str(port),
        })
        command = ["bash", str(ROOT / "scripts" / "start-web-workbench.sh")]
        try:
            started = subprocess.run(
                command + ["start"], env=env, capture_output=True, text=True,
                timeout=20, check=False,
            )
            launcher_log = state_dir / "cppapplab" / "web-workbench.log"
            log_text = (
                launcher_log.read_text(encoding="utf-8", errors="replace")
                if launcher_log.is_file() else "(launcher log was not created)"
            )
            self.assertEqual(
                started.returncode, 0,
                f"Launcher start failed. stdout:\n{started.stdout}\nstderr:\n{started.stderr}\nlog:\n{log_text}",
            )
            self.assertIn("Web Workbench is ready", started.stdout)
            status = subprocess.run(
                command + ["status"], env=env, capture_output=True, text=True,
                timeout=10, check=False,
            )
            self.assertEqual(
                status.returncode, 0,
                f"Launcher status failed. stdout:\n{status.stdout}\nstderr:\n{status.stderr}",
            )
            self.assertIn("is ready", status.stdout)
            stopped = subprocess.run(
                command + ["stop"], env=env, capture_output=True, text=True,
                timeout=10, check=False,
            )
            self.assertEqual(
                stopped.returncode, 0,
                f"Launcher stop failed. stdout:\n{stopped.stdout}\nstderr:\n{stopped.stderr}",
            )
            self.assertIn("Web Workbench stopped", stopped.stdout)
        finally:
            subprocess.run(
                command + ["stop"], env=env, capture_output=True, text=True,
                timeout=10, check=False,
            )

    def test_source_only_reuse_rechecks_all_foundations_on_linux(self) -> None:
        # Standalone regression uses export-shaped frozen reference fixtures.
        # The cross-OS job uses the actual Windows HTTP export from the same
        # workflow artifact, not a reconstructed or fallback fixture.
        expected = {"w1.hello-world": 1, "w1.calculator": 9, "w2.todo-cli": 19,
                    "w3.task-manager": 31, "w4.quality-gate": 41}
        catalog = self.request("/api/v1/health")["catalog_sha256"]
        exchange_path = os.environ.get("CPPAPPLAB_CI_EXCHANGE_FILE")
        actual = None
        if exchange_path:
            raw = Path(exchange_path).read_bytes()
            actual = json.loads(raw)
            self.assertEqual(set(actual), {"schema_version", "kind", "catalog_sha256", "origin", "tasks"})
            self.assertEqual(actual["catalog_sha256"], catalog)
            self.assertEqual([row["task_id"] for row in actual["tasks"]], list(expected))
            print("[INFO] Actual Windows source-export artifact SHA-256: " + hashlib.sha256(raw).hexdigest())
        elif os.environ.get("CPPAPPLAB_REQUIRE_WINDOWS_EXCHANGE") == "1":
            self.fail("Actual native source exchange artifact is required; no fallback.")

        def exchange(task_id):
            if actual is not None:
                # Deep copy for the negative tests, never modify the source artifact.
                return json.loads(json.dumps(dict(actual, tasks=[
                    row for row in actual["tasks"] if row["task_id"] == task_id])))
            task = self.server.task_by_id[task_id]
            reference = (ROOT / task["learner_template"]).parent / "reference"
            files = []
            for relative in task["source_files"]:
                if task_id == "w1.hello-world":
                    data = b'#include <iostream>\nint main(){std::cout<<"Hello, CppAppLab!\\n";}\n'
                else:
                    path = (reference / "src/task_manager_test.cpp"
                            if task_id == "w4.quality-gate" and relative == "tests/task_manager_test.cpp"
                            else reference / relative)
                    data = path.read_bytes()
                files.append({"path": relative, "content": data.decode("utf-8"),
                              "sha256": hashlib.sha256(data).hexdigest()})
            return {"schema_version": 1, "kind": "cppapplab-source-reuse",
                    "catalog_sha256": catalog, "origin": "native-windows",
                    "tasks": [{"task_id": task_id, "files": files}]}

        with self.assertRaises(urllib.error.HTTPError) as locked:
            self.request("/api/v1/tasks/w1.calculator/source-import", "POST", exchange("w1.calculator"))
        self.assertEqual(locked.exception.code, 409)
        self.assertFalse((self.workspace_root / "w1.calculator").exists())
        for mutation in ("hash", "path", "pass"):
            bad = exchange("w1.hello-world")
            if mutation == "hash":
                bad["tasks"][0]["files"][0]["sha256"] = "0" * 64
            elif mutation == "path":
                bad["tasks"][0]["files"][0]["path"] = "../outside.cpp"
            else:
                bad["passed"] = True
            with self.assertRaises(urllib.error.HTTPError) as rejected:
                self.request("/api/v1/tasks/w1.hello-world/source-import", "POST", bad)
            self.assertEqual(rejected.exception.code, 400)
            self.assertFalse((self.workspace_root / "w1.hello-world").exists())
        total = 0
        for task_id, count in expected.items():
            body = exchange(task_id)
            route = "/api/v1/tasks/" + task_id + "/source-import"
            plan = self.request("/api/v1/tasks/w5.system-lab/prerequisite-plan")
            self.assertEqual(plan["next_task"], task_id)
            result = self.request(route, "POST", body)
            self.assertTrue(result["requires_acceptance"])
            self.assertFalse(result["progress_transferred"])
            self.assertNotEqual(self.request("/api/v1/tasks/" + task_id)["state"], "PASSED")
            work = self.workspace_root / task_id
            self.assertFalse((work / ".cppapplab/evidence.txt").exists())
            for item in body["tasks"][0]["files"]:
                self.assertEqual((work / item["path"]).read_bytes(), item["content"].encode("utf-8"))
            before = {item["path"]: (work / item["path"]).read_bytes()
                      for item in body["tasks"][0]["files"]}
            with self.assertRaises(urllib.error.HTTPError) as occupied:
                self.request(route, "POST", body)
            self.assertEqual(occupied.exception.code, 409)
            self.assertEqual(before, {name: (work / name).read_bytes() for name in before})
            checked = self.request("/api/v1/workspaces/" + task_id + "/check", "POST", {})
            self.assertTrue(checked["passed"], checked)
            self.assertIn(f"Acceptance: {count}/{count}", checked["summary"])
            self.assertTrue((work / ".cppapplab/evidence.txt").is_file())
            self.assertEqual(self.request("/api/v1/tasks/" + task_id)["state"], "PASSED")
            total += count
        self.assertEqual(total, 101)
        final = self.request("/api/v1/tasks/w5.system-lab/prerequisite-plan")
        self.assertEqual(final["next_task"], "w5.system-lab")
        self.assertEqual(final["target_state"], "AVAILABLE")
        if actual is not None:
            print("[PASS] Actual Windows exported learner bytes -> Linux 101/101; W5 unlocks only after Linux checks.")
        else:
            print("[PASS] Linux source reuse: 101 real fixed acceptance contracts; W5 unlocks only after Linux checks.")

    def test_linux_prerequisite_plan_follows_real_acceptance_and_stale_source(self) -> None:
        route = "/api/v1/tasks/w5.system-lab/prerequisite-plan"
        initial = self.request(route)
        self.assertEqual(initial["execution_environment"], "linux")
        self.assertEqual(initial["instance_id"], self.request("/api/v1/health")["instance_id"])
        self.assertEqual(initial["target_state"], "LOCKED")
        self.assertEqual(initial["next_task"], "w1.hello-world")
        self.assertEqual(len(initial["pending_prerequisites"]), 5)
        self.request("/api/v1/tasks/w1.hello-world/start", method="POST", body={})
        source = "/api/v1/workspaces/w1.hello-world/files/src/main.cpp"
        self.request(source, method="PUT", body={"content": '#include <iostream>\nint main(){std::cout<<"Hello, CppAppLab!\\n";}\n'})
        # Only the real existing Linux engine check advances the route.
        self.assertEqual(self.request(route)["next_task"], "w1.hello-world")
        checked = self.request("/api/v1/workspaces/w1.hello-world/check", method="POST", body={})
        self.assertTrue(checked["passed"], checked)
        after = self.request(route)
        self.assertEqual(after["next_task"], "w1.calculator")
        self.assertEqual(len(after["pending_prerequisites"]), 4)
        self.request(source, method="PUT", body={"content": "int main(){return 0;}\n"})
        stale = self.request(route)
        self.assertEqual(stale["next_task"], "w1.hello-world")
        self.assertEqual(stale["pending_prerequisites"][0]["state"], "STALE")

    def test_w1_calculator_build_run_and_acceptance_are_separate(self) -> None:
        self.request("/api/v1/tasks/w1.hello-world/start", method="POST", body={})
        self.request(
            "/api/v1/workspaces/w1.hello-world/files/src/main.cpp",
            method="PUT",
            body={"content": (
                "#include <iostream>\n"
                "int main() {\n"
                '    std::cout << "Hello, CppAppLab!\\n";\n'
                "    return 0;\n"
                "}\n"
            )},
        )
        hello_check = self.request(
            "/api/v1/workspaces/w1.hello-world/check", method="POST", body={}
        )
        self.assertTrue(hello_check["passed"], hello_check["output"])

        started = self.request("/api/v1/tasks/w1.calculator/start", method="POST", body={})
        self.assertTrue(started["workspace_ready"])
        calculator_source = (
            '#include "calculator.hpp"\n'
            "namespace cppapplab::calculator {\n"
            "Result calculate(Operation operation, double left, double right) noexcept {\n"
            "    switch (operation) {\n"
            "    case Operation::Add: return {left + right, Error::None};\n"
            "    case Operation::Subtract: return {left - right, Error::None};\n"
            "    case Operation::Multiply: return {left * right, Error::None};\n"
            "    case Operation::Divide:\n"
            "        if (right == 0.0) return {0.0, Error::DivisionByZero};\n"
            "        return {left / right, Error::None};\n"
            "    }\n"
            "    return {0.0, Error::InvalidOperation};\n"
            "}\n"
            "}  // namespace cppapplab::calculator\n"
        )
        self.request(
            "/api/v1/workspaces/w1.calculator/files/src/calculator.cpp",
            method="PUT",
            body={"content": calculator_source},
        )

        build = self.request(
            "/api/v1/workspaces/w1.calculator/build", method="POST", body={}
        )
        self.assertTrue(build["passed"], build["output"])
        self.assertFalse(build["records_acceptance_evidence"])

        self.request(
            "/api/v1/workspaces/w1.calculator/files/src/calculator.cpp",
            method="PUT",
            body={"content": calculator_source + "// edit after build\n"},
        )
        stale_run = self.request(
            "/api/v1/workspaces/w1.calculator/run", method="POST", body={}
        )
        self.assertFalse(stale_run["passed"])
        self.assertIn("Build is missing or stale", stale_run["output"])

        rebuild = self.request(
            "/api/v1/workspaces/w1.calculator/build", method="POST", body={}
        )
        self.assertTrue(rebuild["passed"], rebuild["output"])
        run = self.request(
            "/api/v1/workspaces/w1.calculator/run", method="POST", body={}
        )
        self.assertTrue(run["passed"], run["output"])
        self.assertIn("100% tests passed, 0 tests failed out of 9", run["output"])
        self.assertFalse(run["records_acceptance_evidence"])

        progress = self.request("/api/v1/tasks")
        calculator = next(task for task in progress["tasks"] if task["id"] == "w1.calculator")
        self.assertEqual(calculator["state"], "IN PROGRESS")

        acceptance = self.request(
            "/api/v1/workspaces/w1.calculator/check", method="POST", body={}
        )
        self.assertTrue(acceptance["passed"], acceptance["output"])
        passed = self.request("/api/v1/tasks")
        calculator = next(task for task in passed["tasks"] if task["id"] == "w1.calculator")
        self.assertEqual(calculator["state"], "PASSED")

    def test_w1_run_without_build_reports_the_build_command(self) -> None:
        task_id = "w1.hello-world"
        started = self.request(f"/api/v1/tasks/{task_id}/start", method="POST", body={})
        self.assertTrue(started["workspace_ready"])
        history_path = f"/api/v1/tasks/{task_id}/history"
        history_before = self.request(history_path)
        self.assertEqual(history_before["records"], [])
        state_directory = self.workspace_root / task_id / ".cppapplab"

        for missing_path in (state_directory, state_directory / "build"):
            with self.subTest(missing_path=missing_path.name):
                self.assertFalse(missing_path.exists())
                run = self.request(
                    f"/api/v1/workspaces/{task_id}/run", method="POST", body={}
                )
                self.assertEqual(run["task_id"], task_id)
                self.assertEqual(run["action"], "run")
                self.assertFalse(run["passed"])
                self.assertNotEqual(run["exit_code"], 0)
                self.assertFalse(run["records_acceptance_evidence"])
                self.assertIn("cppapplab build <workspace-directory>", run["output"])
                self.assertFalse(missing_path.exists())
                self.assertEqual(self.request(history_path), history_before)

            if missing_path == state_directory:
                state_directory.mkdir()

    def test_w1_run_rejects_unsafe_build_paths(self) -> None:
        task_id = "w1.hello-world"
        started = self.request(f"/api/v1/tasks/{task_id}/start", method="POST", body={})
        self.assertTrue(started["workspace_ready"])
        history_path = f"/api/v1/tasks/{task_id}/history"
        history_before = self.request(history_path)
        state_directory = self.workspace_root / task_id / ".cppapplab"
        existing_target = self.base / "outside-build"
        existing_target.mkdir()
        missing_target = self.base / "missing-target"

        for unsafe_path in (state_directory, state_directory / "build"):
            for kind, target in (("file", None), ("symlink", existing_target),
                                 ("dangling symlink", missing_target)):
                with self.subTest(path=unsafe_path.name, kind=kind):
                    if target is None:
                        unsafe_path.touch()
                    else:
                        unsafe_path.symlink_to(target, target_is_directory=True)
                    try:
                        run = self.request(
                            f"/api/v1/workspaces/{task_id}/run", method="POST", body={}
                        )
                        self.assertFalse(run["passed"])
                        self.assertNotEqual(run["exit_code"], 0)
                        self.assertFalse(run["records_acceptance_evidence"])
                        self.assertIn("Expected a real, non-symlink build directory", run["output"])
                        self.assertEqual(self.request(history_path), history_before)
                    finally:
                        unsafe_path.unlink()

            if unsafe_path == state_directory:
                state_directory.mkdir()

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

        build = self.request(
            "/api/v1/workspaces/w1.hello-world/build", method="POST", body={}
        )
        self.assertTrue(build["passed"], build["output"])
        self.assertFalse(build["records_acceptance_evidence"])
        run = self.request(
            "/api/v1/workspaces/w1.hello-world/run", method="POST", body={}
        )
        self.assertTrue(run["passed"], run["output"])
        self.assertFalse(run["records_acceptance_evidence"])
        self.assertIn("Hello, CppAppLab!", run["output"])
        in_progress_tasks = self.request("/api/v1/tasks")
        hello = next(task for task in in_progress_tasks["tasks"] if task["id"] == "w1.hello-world")
        self.assertEqual(hello["state"], "IN PROGRESS")

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

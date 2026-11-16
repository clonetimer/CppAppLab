from __future__ import annotations

import json
import os
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from cppapplab_web import CppAppLabWebServer, safe_relative_path


FAKE_ENGINE = """#!/usr/bin/env python3
import os
import pathlib
import sys

command = sys.argv[1] if len(sys.argv) > 1 else ""
home = pathlib.Path(os.environ["HOME"])
task_id = sys.argv[2] if len(sys.argv) > 2 else "w1.hello-world"
workspace = home / "cppapplab-work" / task_id

if command == "--version":
    print("CppAppLab test engine 0.0")
elif command == "progress":
    print("[IN PROGRESS] w1.hello-world · W1 · Hello World")
elif command == "learn":
    (workspace / "src").mkdir(parents=True, exist_ok=True)
    source = workspace / "src" / "main.cpp"
    if not source.exists():
        source.write_text('#include <iostream>\\nint main() { return 0; }\\n', encoding="utf-8")
    print("Created your learner workspace")
    print("Workspace: " + str(workspace))
elif command == "check":
    print("[PASS] fixed acceptance")
else:
    print("unknown command", file=sys.stderr)
    raise SystemExit(2)
"""


class WebWorkbenchTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.base = Path(self.temporary.name)
        self.root = self.base / "repo"
        self.home = self.base / "home"
        self.workspace_root = self.home / "cppapplab-work"
        self.ui_root = self.root / "web" / "ui"
        self.root.mkdir()
        self.home.mkdir()
        self.workspace_root.mkdir(parents=True)
        (self.root / "course" / "lessons" / "w1").mkdir(parents=True)
        self.ui_root.mkdir(parents=True)
        (self.root / "labs" / "w1" / "hello-world" / "learner" / "src").mkdir(parents=True)
        (self.root / "course" / "lessons" / "w1" / "hello-world.md").write_text(
            "# Hello World\n\nWrite your first C++ program.\n", encoding="utf-8"
        )
        (self.root / "course" / "lessons" / "w1" / "hello-world.hints.md").write_text(
            "## Hint 1 — Start\n\nUse main.\n\n## Hint 2 — Output\n\nUse cout.\n\n## Hint 3 — Build\n\nReturn zero.\n",
            encoding="utf-8",
        )
        (self.ui_root / "index.html").write_text(
            "<!doctype html><title>CppAppLab</title>", encoding="utf-8"
        )
        (self.root / "course" / "tasks.tsv").write_text(
            "# catalog\n"
            "w1.hello-world\tW1 · First C++ Program\t1\t1\t"
            "course/lessons/w1/hello-world.md\t"
            "course/lessons/w1/hello-world.hints.md\t"
            "labs/w1/hello-world/learner\tsrc/main.cpp\texact_stdout\t"
            "Hello, CppAppLab!\\n\t\t0\t\n",
            encoding="utf-8",
        )
        self.binary = self.base / "cppapplab"
        self.binary.write_text(FAKE_ENGINE, encoding="utf-8")
        self.binary.chmod(0o755)
        self.server = CppAppLabWebServer(
            ("127.0.0.1", 0),
            self.root,
            self.binary,
            self.workspace_root,
            self.ui_root,
        )
        self.server.child_env["HOME"] = str(self.home)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.base_url = f"http://127.0.0.1:{self.server.server_port}"
        self.token = self.request("/api/v1/session")["csrf_token"]

    def tearDown(self) -> None:
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=3)
        self.temporary.cleanup()

    def request(
        self,
        path: str,
        method: str = "GET",
        body: dict | None = None,
        token: str | None = None,
        origin: str | None = None,
    ):
        data = None if body is None else json.dumps(body).encode("utf-8")
        headers = {}
        if body is not None:
            headers["Content-Type"] = "application/json"
        if token is not None:
            headers["X-CppAppLab-Token"] = token
        if origin is not None:
            headers["Origin"] = origin
        request = urllib.request.Request(
            self.base_url + path, data=data, headers=headers, method=method
        )
        with urllib.request.urlopen(request, timeout=5) as response:
            raw = response.read()
            if response.headers.get_content_type() == "application/json":
                return json.loads(raw.decode("utf-8"))
            return raw.decode("utf-8")

    def request_error(
        self,
        path: str,
        method: str,
        body: dict | None = None,
        token: str | None = None,
        origin: str | None = None,
    ) -> int:
        data = None if body is None else json.dumps(body).encode("utf-8")
        headers = {}
        if body is not None:
            headers["Content-Type"] = "application/json"
        if token is not None:
            headers["X-CppAppLab-Token"] = token
        if origin is not None:
            headers["Origin"] = origin
        request = urllib.request.Request(
            self.base_url + path, data=data, headers=headers, method=method
        )
        try:
            urllib.request.urlopen(request, timeout=5)
        except urllib.error.HTTPError as error:
            return error.code
        self.fail("Expected an HTTP error response.")
        return 0

    def test_catalog_and_lesson_are_served_from_authoritative_course(self) -> None:
        tasks = self.request("/api/v1/tasks")
        self.assertEqual(tasks["count"], 1)
        self.assertEqual(tasks["tasks"][0]["id"], "w1.hello-world")
        self.assertEqual(tasks["tasks"][0]["state"], "IN PROGRESS")
        task = self.request("/api/v1/tasks/w1.hello-world")
        self.assertIn("Write your first C++ program", task["lesson_markdown"])
        self.assertEqual(task["source_files"], ["src/main.cpp"])

    def test_start_edit_save_and_fixed_check_workflow(self) -> None:
        started = self.request(
            "/api/v1/tasks/w1.hello-world/start",
            method="POST",
            body={},
            token=self.token,
            origin=self.base_url,
        )
        self.assertTrue(started["workspace_ready"])
        files = self.request("/api/v1/workspaces/w1.hello-world/files")
        self.assertEqual(files["files"][0]["path"], "src/main.cpp")
        source = self.request("/api/v1/workspaces/w1.hello-world/files/src/main.cpp")
        self.assertIn("int main()", source["content"])
        saved = self.request(
            "/api/v1/workspaces/w1.hello-world/files/src/main.cpp",
            method="PUT",
            body={"content": "#include <iostream>\nint main() { return 0; }\n"},
            token=self.token,
            origin=self.base_url,
        )
        self.assertTrue(saved["saved"])
        reloaded = self.request("/api/v1/workspaces/w1.hello-world/files/src/main.cpp")
        self.assertEqual(reloaded["content"], "#include <iostream>\\nint main() { return 0; }\\n")
        check = self.request(
            "/api/v1/workspaces/w1.hello-world/check",
            method="POST",
            body={},
            token=self.token,
            origin=self.base_url,
        )
        self.assertTrue(check["passed"])
        self.assertIn("fixed acceptance", check["output"])

    def test_progressive_hint_is_extracted(self) -> None:
        hint = self.request("/api/v1/tasks/w1.hello-world/hints/2")
        self.assertEqual(hint["level"], 2)
        self.assertEqual(hint["markdown"], "Use cout.")

    def test_mutations_require_same_origin_and_csrf_token(self) -> None:
        path = "/api/v1/tasks/w1.hello-world/start"
        self.assertEqual(
            self.request_error(path, "POST", {}, token=self.token, origin="http://attacker.invalid"),
            403,
        )
        self.assertEqual(
            self.request_error(path, "POST", {}, token=None, origin=self.base_url),
            403,
        )

    def test_undeclared_source_file_is_rejected(self) -> None:
        self.request(
            "/api/v1/tasks/w1.hello-world/start",
            method="POST",
            body={},
            token=self.token,
            origin=self.base_url,
        )
        self.assertEqual(
            self.request_error(
                "/api/v1/workspaces/w1.hello-world/files/README.md",
                "PUT",
                {"content": "not allowed"},
                token=self.token,
                origin=self.base_url,
            ),
            403,
        )

    def test_symlink_source_file_is_rejected(self) -> None:
        self.request(
            "/api/v1/tasks/w1.hello-world/start",
            method="POST",
            body={},
            token=self.token,
            origin=self.base_url,
        )
        workspace = self.workspace_root / "w1.hello-world"
        source = workspace / "src" / "main.cpp"
        source.unlink()
        source.symlink_to("/etc/passwd")
        self.assertEqual(
            self.request_error(
                "/api/v1/workspaces/w1.hello-world/files/src/main.cpp",
                "GET",
            ),
            400,
        )

    def test_unknown_command_route_is_not_exposed(self) -> None:
        self.assertEqual(
            self.request_error(
                "/api/v1/command",
                "POST",
                {"command": "rm -rf /"},
                token=self.token,
                origin=self.base_url,
            ),
            404,
        )

    def test_static_shell_is_served(self) -> None:
        page = self.request("/")
        self.assertIn("CppAppLab", page)


class PathValidationTests(unittest.TestCase):
    def test_accepts_normalized_relative_source_path(self) -> None:
        self.assertEqual(safe_relative_path("src/main.cpp"), "src/main.cpp")

    def test_rejects_absolute_traversal_and_windows_paths(self) -> None:
        for value in ("../secret", "src/../../secret", "/etc/passwd", r"src\main.cpp", "C:/secret"):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    safe_relative_path(value)


if __name__ == "__main__":
    unittest.main()

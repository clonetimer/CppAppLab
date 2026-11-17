from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import urllib.error
import urllib.request
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from cppapplab_web import CppAppLabWebServer, ServiceSession, safe_relative_path


FAKE_ENGINE = """#!/usr/bin/env python3
import os
import pathlib
import sys
import time
import socket

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
elif command == "build":
    print("Build result: PASS")
elif command == "run":
    print("Run result: PASS")
elif command == "app-run-stdin":
    payload = sys.stdin.buffer.read()
    print("stdin-bytes=" + str(len(payload)))
    print("argv-count=" + str(len(sys.argv)))
elif command == "app-build":
    print("Application build result: PASS")
elif command == "server":
    listener = socket.socket()
    listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    listener.bind(("127.0.0.1", 0))
    listener.listen()
    print("listening on 127.0.0.1:" + str(listener.getsockname()[1]), flush=True)
    time.sleep(30)
elif command == "check":
    print("[PASS] fixed acceptance")
else:
    print("unknown command", file=sys.stderr)
    raise SystemExit(2)
"""


FAKE_CHAT_SERVER = """#!/usr/bin/env python3
import socket
import sys
import threading

listener = socket.socket()
listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
listener.bind(("127.0.0.1", int(sys.argv[2])))
listener.listen()
clients = {}
lock = threading.Lock()
print("listening on 127.0.0.1:" + str(listener.getsockname()[1]), flush=True)

def broadcast(message, excluded=None):
    with lock:
        targets = list(clients.items())
    for connection, nickname in targets:
        if connection is excluded or not nickname:
            continue
        try:
            connection.sendall(message.encode("utf-8"))
        except OSError:
            pass

def handle(connection):
    nickname = ""
    buffer = ""
    try:
        while True:
            data = connection.recv(4096)
            if not data:
                break
            buffer += data.decode("utf-8", errors="replace")
            while "\\n" in buffer:
                line, buffer = buffer.split("\\n", 1)
                line = line.rstrip("\\r")
                if line.startswith("NICK "):
                    nickname = line[5:]
                    with lock:
                        clients[connection] = nickname
                    connection.sendall(("OK NICK " + nickname + "\\n").encode("utf-8"))
                    broadcast("* " + nickname + " joined\\n", connection)
                elif line.startswith("MSG ") and nickname:
                    broadcast(nickname + ": " + line[4:] + "\\n", connection)
                elif line == "QUIT":
                    return
    except OSError:
        pass
    finally:
        with lock:
            clients.pop(connection, None)
        try:
            connection.close()
        except OSError:
            pass
        if nickname:
            broadcast("* " + nickname + " left\\n")

while True:
    connection, _ = listener.accept()
    threading.Thread(target=handle, args=(connection,), daemon=True).start()
"""


FAKE_HTTP_SERVER = """#!/usr/bin/env python3
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import sys

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        body = b"x" * (256 * 1024 + 1) if self.path == "/large" else b"browser proxy ok"
        self.send_response(200)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)
        self.send_response(201)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

server = ThreadingHTTPServer(("127.0.0.1", int(sys.argv[2])), Handler)
print("listening on " + str(server.server_port), flush=True)
server.serve_forever()
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
        self.assertEqual(reloaded["content"], "#include <iostream>\nint main() { return 0; }\n")
        check = self.request(
            "/api/v1/workspaces/w1.hello-world/check",
            method="POST",
            body={},
            token=self.token,
            origin=self.base_url,
        )
        self.assertTrue(check["passed"])
        self.assertIn("fixed acceptance", check["output"])

    def test_build_and_run_are_separate_from_fixed_acceptance(self) -> None:
        self.request(
            "/api/v1/tasks/w1.hello-world/start",
            method="POST",
            body={},
            token=self.token,
            origin=self.base_url,
        )
        build = self.request(
            "/api/v1/workspaces/w1.hello-world/build",
            method="POST",
            body={},
            token=self.token,
            origin=self.base_url,
        )
        self.assertTrue(build["passed"])
        self.assertFalse(build["records_acceptance_evidence"])
        self.assertEqual(build["action"], "build")
        run = self.request(
            "/api/v1/workspaces/w1.hello-world/run",
            method="POST",
            body={},
            token=self.token,
            origin=self.base_url,
        )
        self.assertTrue(run["passed"])
        self.assertFalse(run["records_acceptance_evidence"])
        self.assertEqual(run["action"], "run")
        tasks = self.request("/api/v1/tasks")
        self.assertEqual(tasks["tasks"][0]["state"], "IN PROGRESS")

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

    def test_http_input_uses_stdin_not_argv(self) -> None:
        request_body = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
        workspace = self.workspace_root / "w8.http-parser"
        with patch.object(
            self.server, "workspace_for_task", return_value=(workspace, "")
        ), patch.object(
            self.server, "run_cli", return_value=(1, "learner TODO")
        ) as engine:
            result = self.request(
                "/api/v1/workspaces/w8.http-parser/app-run",
                method="POST",
                body={"action": "parse", "values": [request_body]},
                token=self.token,
                origin=self.base_url,
            )
            self.assertFalse(result["success"])
            self.assertFalse(result["records_evidence"])
            self.assertEqual(result["output"], "learner TODO")
            args, kwargs = engine.call_args
            self.assertEqual(
                args[0], ["app-run-stdin", str(workspace), "parse"]
            )
            self.assertEqual(kwargs["input_data"], request_body)
            self.assertNotIn(request_body, str(args))

    def test_large_http_input_reaches_real_subprocess_stdin(self) -> None:
        # Linux caps individual argv strings at around 128 KiB. This regression
        # must pass with a payload larger than that cap, without mocking the
        # subprocess runner or injecting the payload into argv.
        request_body = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" + ("x" * 140000)
        workspace = self.workspace_root / "w8.http-parser"
        with patch.object(
            self.server, "workspace_for_task", return_value=(workspace, "")
        ):
            result = self.request(
                "/api/v1/workspaces/w8.http-parser/app-run",
                method="POST",
                body={"action": "parse", "values": [request_body]},
                token=self.token,
                origin=self.base_url,
            )
        self.assertTrue(result["success"])
        self.assertFalse(result["records_evidence"])
        self.assertIn(
            f"stdin-bytes={len(request_body.encode('utf-8'))}",
            result["output"],
        )
        self.assertIn("argv-count=4", result["output"])

    def test_http_input_rejects_unsupported_action(self) -> None:
        with patch.object(self.server, "run_cli") as engine:
            response = self.request_error(
                "/api/v1/workspaces/w8.http-parser/app-run",
                "POST",
                {"action": "shell", "values": ["/bin/sh -c whoami"]},
                token=self.token,
                origin=self.base_url,
            )
            self.assertEqual(response, 400)
            engine.assert_not_called()

    def test_http_input_enforces_256_kib_size_limit(self) -> None:
        with patch.object(self.server, "run_cli") as engine:
            response = self.request_error(
                "/api/v1/workspaces/w8.http-parser/app-run",
                "POST",
                {"action": "parse", "values": ["x" * (256 * 1024 + 1)]},
                token=self.token,
                origin=self.base_url,
            )
            self.assertEqual(response, 400)
            engine.assert_not_called()

    def test_unregistered_application_profile_is_rejected(self) -> None:
        response = self.request_error(
            "/api/v1/workspaces/w8.http-keep-alive/app-run",
            "POST",
            {"action": "parse", "values": ["GET / HTTP/1.1"]},
            token=self.token,
            origin=self.base_url,
        )
        self.assertEqual(response, 409)

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

    def test_service_supervisor_starts_reports_and_stops_registered_loopback_process(self) -> None:
        workspace = self.workspace_root / "w7.tcp-echo"
        executable = workspace / ".cppapplab" / "application" / "learner_app"
        executable.parent.mkdir(parents=True, exist_ok=True)
        executable.write_text(FAKE_ENGINE, encoding="utf-8")
        executable.chmod(0o755)
        with patch.object(
            self.server, "workspace_for_task", return_value=(workspace, "")
        ), patch.object(
            self.server, "run_cli", return_value=(0, "Application build result: PASS")
        ):
            started = self.request(
                "/api/v1/workspaces/w7.tcp-echo/service/start",
                method="POST",
                body={},
                token=self.token,
                origin=self.base_url,
            )
            self.assertEqual(started["state"], "RUNNING")
            self.assertEqual(started["kind"], "tcp_echo")
            self.assertGreaterEqual(started["port"], 1)
            self.assertLessEqual(started["port"], 65535)
            session = self.server.service_sessions["w7.tcp-echo"]
            deadline = time.monotonic() + 1.0
            while time.monotonic() < deadline:
                status = self.request("/api/v1/workspaces/w7.tcp-echo/service/status")
                if f"listening on 127.0.0.1:{started['port']}" in status["logs"]:
                    break
                time.sleep(0.01)
            self.assertIn(f"listening on 127.0.0.1:{started['port']}", status["logs"])
            self.assertEqual(
                Path(self.server.service_sessions["w7.tcp-echo"].process.args[0]),
                executable,
            )

            session = self.server.service_sessions["w7.tcp-echo"]
            self.assertIsNone(session.process.poll())
            status = self.request("/api/v1/workspaces/w7.tcp-echo/service/status")
            self.assertEqual(status["state"], "RUNNING")
            self.assertEqual(status["port"], started["port"])

            stopped = self.request(
                "/api/v1/workspaces/w7.tcp-echo/service/stop",
                method="POST",
                body={},
                token=self.token,
                origin=self.base_url,
            )
            self.assertEqual(stopped["state"], "STOPPED")
            self.assertIsNotNone(session.process.poll())
            self.assertIn("Stopped by learner", stopped["logs"])

    def test_tcp_echo_bridge_uses_the_registered_client_and_session_port(self) -> None:
        workspace = self.workspace_root / "w7.tcp-echo"
        workspace.mkdir(parents=True, exist_ok=True)
        runtime = workspace / ".cppapplab" / "application" / "runtime"
        runtime.mkdir(parents=True, exist_ok=True)
        executable = runtime.parent / "learner_app"
        executable.write_text(FAKE_ENGINE, encoding="utf-8")
        executable.chmod(0o755)
        with patch.object(
            self.server, "workspace_for_task", return_value=(workspace, "")
        ), patch.object(
            self.server, "run_cli",
            side_effect=[
                (0, "Application build result: PASS"),
                (0, "echoed: hello from browser"),
            ],
        ) as engine:
            started = self.request(
                "/api/v1/workspaces/w7.tcp-echo/service/start",
                method="POST",
                body={},
                token=self.token,
                origin=self.base_url,
            )
            self.assertEqual(started["state"], "RUNNING")
            echoed = self.request(
                "/api/v1/workspaces/w7.tcp-echo/service/echo",
                method="POST",
                body={"message": "hello from browser"},
                token=self.token,
                origin=self.base_url,
            )
        self.assertTrue(echoed["success"])
        self.assertIn("echoed: hello from browser", echoed["output"])
        self.assertEqual(
            engine.call_args_list[1].args[0],
            ["app-run", str(workspace), "client", str(started["port"]), "hello from browser"],
        )

    def test_tcp_echo_bridge_rejects_oversized_and_multiline_messages(self) -> None:
        workspace = self.workspace_root / "w7.tcp-echo"
        workspace.mkdir(parents=True, exist_ok=True)
        runtime = workspace / ".cppapplab" / "application" / "runtime"
        runtime.mkdir(parents=True, exist_ok=True)
        executable = runtime.parent / "learner_app"
        executable.write_text(FAKE_ENGINE, encoding="utf-8")
        executable.chmod(0o755)
        with patch.object(
            self.server, "workspace_for_task", return_value=(workspace, "")
        ), patch.object(
            self.server, "run_cli", return_value=(0, "Application build result: PASS")
        ) as engine:
            self.request(
                "/api/v1/workspaces/w7.tcp-echo/service/start",
                method="POST",
                body={},
                token=self.token,
                origin=self.base_url,
            )
            self.assertEqual(
                self.request_error(
                    "/api/v1/workspaces/w7.tcp-echo/service/echo",
                    "POST",
                    {"message": "x" * 1025},
                    token=self.token,
                    origin=self.base_url,
                ),
                400,
            )
            self.assertEqual(
                self.request_error(
                    "/api/v1/workspaces/w7.tcp-echo/service/echo",
                    "POST",
                    {"message": "hello\nworld"},
                    token=self.token,
                    origin=self.base_url,
                ),
                400,
            )
        self.assertEqual(engine.call_count, 1, "Invalid echo messages must not launch a learner client.")

    def test_tcp_chat_bridge_manages_two_clients_and_broadcasts_messages(self) -> None:
        workspace = self.workspace_root / "w7.tcp-chat"
        executable = workspace / ".cppapplab" / "application" / "learner_app"
        executable.parent.mkdir(parents=True, exist_ok=True)
        executable.write_text(FAKE_CHAT_SERVER, encoding="utf-8")
        executable.chmod(0o755)
        with patch.object(
            self.server, "workspace_for_task", return_value=(workspace, "")
        ), patch.object(
            self.server, "run_cli", return_value=(0, "Application build result: PASS")
        ):
            started = self.request(
                "/api/v1/workspaces/w7.tcp-chat/service/start",
                method="POST", body={}, token=self.token, origin=self.base_url,
            )
            self.assertEqual(started["state"], "RUNNING")
            alice = self.request(
                "/api/v1/workspaces/w7.tcp-chat/service/chat/connect",
                method="POST", body={"nickname": "Alice"}, token=self.token, origin=self.base_url,
            )
            bob = self.request(
                "/api/v1/workspaces/w7.tcp-chat/service/chat/connect",
                method="POST", body={"nickname": "Bob"}, token=self.token, origin=self.base_url,
            )
            self.assertIn("OK NICK Alice", alice["messages"][alice["client_id"]])
            self.assertIn("* Bob joined", bob["messages"][alice["client_id"]])
            self.assertIn("OK NICK Bob", bob["messages"][bob["client_id"]])

            sent = self.request(
                "/api/v1/workspaces/w7.tcp-chat/service/chat/send",
                method="POST",
                body={"client_id": alice["client_id"], "message": "hello Bob"},
                token=self.token, origin=self.base_url,
            )
            self.assertIn("Alice: hello Bob", sent["messages"][bob["client_id"]])
            self.assertEqual(sent["service"]["clients_count"], 2)

            disconnected = self.request(
                "/api/v1/workspaces/w7.tcp-chat/service/chat/disconnect",
                method="POST",
                body={"client_id": bob["client_id"]},
                token=self.token, origin=self.base_url,
            )
            self.assertIn("* Bob left", disconnected["messages"][alice["client_id"]])
            self.assertEqual(disconnected["service"]["clients_count"], 1)

            stopped = self.request(
                "/api/v1/workspaces/w7.tcp-chat/service/stop",
                method="POST", body={}, token=self.token, origin=self.base_url,
            )
            self.assertEqual(stopped["state"], "STOPPED")
            self.assertEqual(stopped["clients_count"], 0)

    def test_tcp_chat_bridge_rejects_invalid_nicknames_and_messages(self) -> None:
        workspace = self.workspace_root / "w7.tcp-chat"
        executable = workspace / ".cppapplab" / "application" / "learner_app"
        executable.parent.mkdir(parents=True, exist_ok=True)
        executable.write_text(FAKE_CHAT_SERVER, encoding="utf-8")
        executable.chmod(0o755)
        with patch.object(
            self.server, "workspace_for_task", return_value=(workspace, "")
        ), patch.object(
            self.server, "run_cli", return_value=(0, "Application build result: PASS")
        ):
            self.request(
                "/api/v1/workspaces/w7.tcp-chat/service/start",
                method="POST", body={}, token=self.token, origin=self.base_url,
            )
            self.assertEqual(
                self.request_error(
                    "/api/v1/workspaces/w7.tcp-chat/service/chat/connect",
                    "POST", {"nickname": "bad name"}, token=self.token, origin=self.base_url,
                ),
                400,
            )
            alice = self.request(
                "/api/v1/workspaces/w7.tcp-chat/service/chat/connect",
                method="POST", body={"nickname": "Alice"}, token=self.token, origin=self.base_url,
            )
            self.assertEqual(
                self.request_error(
                    "/api/v1/workspaces/w7.tcp-chat/service/chat/send",
                    "POST",
                    {"client_id": alice["client_id"], "message": "x" * 1025},
                    token=self.token, origin=self.base_url,
                ),
                400,
            )

    def test_http_service_proxy_uses_loopback_and_bounds_request_data(self) -> None:
        workspace = self.workspace_root / "w8.http-server"
        executable = workspace / ".cppapplab" / "application" / "learner_app"
        executable.parent.mkdir(parents=True, exist_ok=True)
        executable.write_text(FAKE_HTTP_SERVER, encoding="utf-8")
        executable.chmod(0o755)
        with patch.object(
            self.server, "workspace_for_task", return_value=(workspace, "")
        ), patch.object(
            self.server, "run_cli", return_value=(0, "Application build result: PASS")
        ):
            started = self.request(
                "/api/v1/workspaces/w8.http-server/service/start",
                method="POST", body={}, token=self.token, origin=self.base_url,
            )
            self.assertEqual(started["state"], "RUNNING")
            get_result = self.request(
                "/api/v1/workspaces/w8.http-server/service/http",
                method="POST",
                body={"method": "GET", "target": "/", "body": ""},
                token=self.token, origin=self.base_url,
            )
            self.assertEqual(get_result["status_code"], 200)
            self.assertEqual(get_result["body"], "browser proxy ok")
            self.assertEqual(get_result["headers"]["content-type"], "text/plain; charset=utf-8")

            post_result = self.request(
                "/api/v1/workspaces/w8.http-server/service/http",
                method="POST",
                body={"method": "POST", "target": "/echo", "body": "hello from browser"},
                token=self.token, origin=self.base_url,
            )
            self.assertEqual(post_result["status_code"], 201)
            self.assertEqual(post_result["body"], "hello from browser")

            self.assertEqual(
                self.request_error(
                    "/api/v1/workspaces/w8.http-server/service/http",
                    "POST", {"method": "GET", "target": "http://example.com/", "body": ""},
                    token=self.token, origin=self.base_url,
                ),
                400,
            )
            self.assertEqual(
                self.request_error(
                    "/api/v1/workspaces/w8.http-server/service/http",
                    "POST", {"method": "GET", "target": "/%2e%2e/secret", "body": ""},
                    token=self.token, origin=self.base_url,
                ),
                400,
            )
            self.assertEqual(
                self.request_error(
                    "/api/v1/workspaces/w8.http-server/service/http",
                    "POST", {"method": "GET", "target": "/%0d%0aHost:evil", "body": ""},
                    token=self.token, origin=self.base_url,
                ),
                400,
            )
            self.assertEqual(
                self.request_error(
                    "/api/v1/workspaces/w8.http-server/service/http",
                    "POST", {"method": "GET", "target": "/large", "body": ""},
                    token=self.token, origin=self.base_url,
                ),
                413,
            )
            self.assertEqual(
                self.request_error(
                    "/api/v1/workspaces/w8.http-server/service/http",
                    "POST", {"method": "GET", "target": "/", "body": "not allowed"},
                    token=self.token, origin=self.base_url,
                ),
                400,
            )
            self.assertEqual(
                self.request_error(
                    "/api/v1/workspaces/w8.http-server/service/http",
                    "POST", {"method": "GET", "target": "/", "body": "x" * (64 * 1024 + 1)},
                    token=self.token, origin=self.base_url,
                ),
                413,
            )
            stopped = self.request(
                "/api/v1/workspaces/w8.http-server/service/stop",
                method="POST", body={}, token=self.token, origin=self.base_url,
            )
            self.assertEqual(stopped["state"], "STOPPED")

    def test_service_build_failure_does_not_launch_a_child_process(self) -> None:
        workspace = self.workspace_root / "w7.tcp-chat"
        workspace.mkdir(parents=True, exist_ok=True)
        with patch.object(
            self.server, "workspace_for_task", return_value=(workspace, "")
        ), patch.object(
            self.server, "run_cli", return_value=(1, "learner compile error")
        ), patch("cppapplab_web.subprocess.Popen") as process:
            result = self.request(
                "/api/v1/workspaces/w7.tcp-chat/service/start",
                method="POST",
                body={},
                token=self.token,
                origin=self.base_url,
            )
        self.assertEqual(result["state"], "BUILD_FAILED")
        self.assertIn("learner compile error", result["logs"])
        process.assert_not_called()
        self.assertNotIn("w7.tcp-chat", self.server.service_sessions)
        refreshed = self.request("/api/v1/workspaces/w7.tcp-chat/service/status")
        self.assertEqual(refreshed["state"], "BUILD_FAILED")
        self.assertIn("learner compile error", refreshed["logs"])

    def test_service_cleanup_kills_descendants_after_parent_exit(self) -> None:
        script = (
            "import subprocess, sys; "
            "child = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(30)']); "
            "print(child.pid, flush=True)"
        )
        process = subprocess.Popen(
            [sys.executable, "-c", script],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            start_new_session=True,
        )
        self.assertIsNotNone(process.stdout)
        child_pid = int(process.stdout.readline().strip())
        self.assertEqual(process.wait(timeout=2.0), 0)
        session = ServiceSession(
            task_id="w7.tcp-echo",
            kind="tcp_echo",
            workspace=self.workspace_root,
            process=process,
            port=0,
        )
        self.assertTrue(CppAppLabWebServer._terminate_service_process_group(session, 1.0))
        deadline = time.monotonic() + 1.0
        while time.monotonic() < deadline:
            try:
                os.kill(child_pid, 0)
            except ProcessLookupError:
                break
            time.sleep(0.02)
        else:
            self.fail("Service cleanup left a descendant process alive.")
        process.stdout.close()

    def test_service_routes_reject_unregistered_application_tasks(self) -> None:
        self.assertEqual(
            self.request_error(
                "/api/v1/workspaces/w1.hello-world/service/start",
                "POST",
                {},
                token=self.token,
                origin=self.base_url,
            ),
            409,
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

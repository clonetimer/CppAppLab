#!/usr/bin/env python3
"""Local browser adapter for the existing CppAppLab learning engine.

The service intentionally exposes fixed learning actions rather than a shell.
It uses only the Python standard library and binds to loopback by default.
"""

from __future__ import annotations

import argparse
import json
import mimetypes
import os
import re
import secrets
import signal
import subprocess
import sys
import tempfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path, PurePosixPath
from typing import Any
from urllib.parse import unquote, urlsplit


MAX_CATALOG_BYTES = 64 * 1024
MAX_LESSON_BYTES = 64 * 1024
MAX_HINT_BYTES = 32 * 1024
MAX_SOURCE_BYTES = 1024 * 1024
MAX_REQUEST_BYTES = MAX_SOURCE_BYTES + 16 * 1024
MAX_PROCESS_OUTPUT = 256 * 1024
TASK_ID_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{0,95}$")
PROGRESS_RE = re.compile(r"^\[(?P<state>[^\]]+)\]\s+(?P<task>[a-z0-9][a-z0-9._-]*)\s+·")
HINT_RE = re.compile(r"^## Hint ([1-3])(?:\s|$)", re.MULTILINE)
ALLOWED_HOSTS = {"127.0.0.1", "localhost", "::1"}

RUNTIME_PROFILES: dict[str, dict[str, Any]] = {
    "w1.calculator": {
        "label": "命令行计算器",
        "actions": [{
            "id": "calculate", "label": "执行计算",
            "fields": [
                {"name": "operation", "label": "运算", "type": "select",
                 "options": ["add", "subtract", "multiply", "divide"]},
                {"name": "left", "label": "左操作数", "type": "number"},
                {"name": "right", "label": "右操作数", "type": "number"},
            ],
        }],
    },
    "w2.todo-cli": {
        "label": "持久化 Todo CLI",
        "actions": [
            {"id": "add", "label": "添加待办", "fields": [{"name": "title", "label": "待办内容", "type": "text"}]},
            {"id": "list", "label": "查看列表", "fields": []},
            {"id": "done", "label": "完成待办", "fields": [{"name": "id", "label": "待办编号", "type": "id"}]},
            {"id": "remove", "label": "删除待办", "fields": [{"name": "id", "label": "待办编号", "type": "id"}]},
        ],
    },
    "w3.task-manager": {
        "label": "TaskManager CLI",
        "actions": [
            {"id": "add", "label": "添加任务", "fields": [{"name": "title", "label": "任务内容", "type": "text"}]},
            {"id": "list", "label": "查看任务", "fields": []},
            {"id": "done", "label": "完成任务", "fields": [{"name": "id", "label": "任务编号", "type": "id"}]},
            {"id": "remove", "label": "删除任务", "fields": [{"name": "id", "label": "任务编号", "type": "id"}]},
        ],
    },
    "w4.quality-gate": {
        "label": "TaskManager Quality Gate CLI",
        "actions": [
            {"id": "add", "label": "添加任务", "fields": [{"name": "title", "label": "任务内容", "type": "text"}]},
            {"id": "list", "label": "查看任务", "fields": []},
            {"id": "done", "label": "完成任务", "fields": [{"name": "id", "label": "任务编号", "type": "id"}]},
            {"id": "remove", "label": "删除任务", "fields": [{"name": "id", "label": "任务编号", "type": "id"}]},
        ],
    },
    "w5.system-lab": {
        "label": "受限进程运行演示",
        "actions": [{"id": "echo", "label": "运行固定 Echo 示例", "fields": []}],
    },
    "w6.thread-pool": {
        "label": "Thread Pool Demo",
        "actions": [{"id": "demo", "label": "运行线程池示例", "fields": []}],
    },
    "w8.http-protocol": {
        "label": "HTTP 协议消息",
        "actions": [
            {"id": "parse-line", "label": "解析请求行",
             "fields": [{"name": "request_line", "label": "HTTP 请求行", "type": "text"}]},
            {"id": "response", "label": "生成示例响应", "fields": []},
        ],
    },
    "w8.static-files": {
        "label": "HTTP 静态文件服务",
        "actions": [{
            "id": "handle", "label": "处理静态文件请求",
            "fields": [
                {"name": "method", "label": "HTTP 方法", "type": "select",
                 "options": ["GET", "HEAD", "POST"]},
                {"name": "target", "label": "请求路径", "type": "text"},
            ],
        }],
    },
    "w8.http-parser": {
        "label": "HTTP 请求解析器",
        "actions": [{
            "id": "parse", "label": "解析 HTTP 请求",
            "fields": [{
                "name": "request", "label": "HTTP 请求报文", "type": "multiline",
                "default": "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n",
                "placeholder": "GET / HTTP/1.1\\r\\nHost: localhost\\r\\n\\r\\n",
            }],
        }],
    },
    "w8.router": {
        "label": "HTTP 路由与分发",
        "actions": [{
            "id": "dispatch", "label": "分发 HTTP 请求",
            "fields": [{
                "name": "request", "label": "HTTP 请求报文", "type": "multiline",
                "default": "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n",
                "placeholder": "GET / HTTP/1.1\\r\\nHost: localhost\\r\\n\\r\\n",
            }],
        }],
    },
}


class ApiError(Exception):
    def __init__(self, status: int, message: str):
        super().__init__(message)
        self.status = status
        self.message = message


def safe_relative_path(value: str) -> str:
    if not value or "\\" in value or "\x00" in value or value.startswith("/"):
        raise ValueError("Expected a non-empty relative POSIX path.")
    path = PurePosixPath(value)
    if not path.parts or any(part in {"", ".", ".."} for part in path.parts):
        raise ValueError("Path traversal and dot components are not allowed.")
    if ":" in path.parts[0]:
        raise ValueError("Drive-qualified paths are not allowed.")
    normalized = path.as_posix()
    if normalized != value:
        raise ValueError("Path must use its normalized POSIX representation.")
    return normalized


def _lstat_regular_file(path: Path, label: str) -> os.stat_result:
    try:
        info = path.lstat()
    except OSError as exc:
        raise ApiError(404, f"{label} was not found.") from exc
    if path.is_symlink() or not path.is_file():
        raise ApiError(400, f"{label} must be a regular non-symlink file.")
    return info


def safe_repo_file(root: Path, relative: str, max_bytes: int, label: str) -> Path:
    try:
        normalized = safe_relative_path(relative)
    except ValueError as exc:
        raise ApiError(400, str(exc)) from exc

    current = root
    for part in PurePosixPath(normalized).parts:
        current = current / part
        try:
            info = current.lstat()
        except OSError as exc:
            raise ApiError(404, f"{label} was not found.") from exc
        if current.is_symlink():
            raise ApiError(400, f"{label} cannot contain symlinks.")
        if current != root / normalized and not current.is_dir():
            raise ApiError(400, f"{label} contains a non-directory parent.")
        if current == root / normalized:
            if not current.is_file():
                raise ApiError(400, f"{label} must be a regular file.")
            if info.st_size > max_bytes:
                raise ApiError(413, f"{label} exceeds its size limit.")
    return current


def read_bounded(path: Path, max_bytes: int, label: str) -> str:
    info = _lstat_regular_file(path, label)
    if info.st_size > max_bytes:
        raise ApiError(413, f"{label} exceeds its size limit.")
    try:
        with path.open("r", encoding="utf-8") as stream:
            value = stream.read(max_bytes + 1)
    except UnicodeDecodeError as exc:
        raise ApiError(400, f"{label} is not valid UTF-8.") from exc
    except OSError as exc:
        raise ApiError(404, f"{label} could not be read.") from exc
    if len(value.encode("utf-8")) > max_bytes:
        raise ApiError(413, f"{label} exceeds its size limit.")
    return value


def load_catalog(root: Path) -> list[dict[str, Any]]:
    catalog_path = safe_repo_file(root, "course/tasks.tsv", MAX_CATALOG_BYTES, "Task catalog")
    raw = read_bounded(catalog_path, MAX_CATALOG_BYTES, "Task catalog")
    tasks: list[dict[str, Any]] = []
    seen: set[str] = set()

    for line_number, line in enumerate(raw.splitlines(), start=1):
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) != 13:
            raise ApiError(500, f"Task catalog row {line_number} has {len(fields)} fields; expected 13.")
        task_id, title, week, order, lesson, hints, template, sources, acceptance, expected, fixture, count, prerequisites = fields
        if not TASK_ID_RE.fullmatch(task_id) or task_id in seen:
            raise ApiError(500, f"Task catalog row {line_number} has an invalid or duplicate task ID.")
        try:
            week_number = int(week)
            order_number = int(order)
            test_count = int(count or "0")
            lesson_path = safe_relative_path(lesson)
            hints_path = safe_relative_path(hints)
            template_path = safe_relative_path(template)
            source_paths = [safe_relative_path(item) for item in sources.split(";") if item]
            fixture_path = safe_relative_path(fixture) if fixture else None
            prerequisite_ids = [item for item in prerequisites.split(";") if item]
        except (ValueError, TypeError) as exc:
            raise ApiError(500, f"Task catalog row {line_number} contains invalid metadata.") from exc
        if week_number < 1 or order_number < 1 or not title or not source_paths:
            raise ApiError(500, f"Task catalog row {line_number} is incomplete.")
        if acceptance not in {"exact_stdout", "cmake_gtest"}:
            raise ApiError(500, f"Task catalog row {line_number} has an unknown acceptance kind.")
        if any(not TASK_ID_RE.fullmatch(item) for item in prerequisite_ids):
            raise ApiError(500, f"Task catalog row {line_number} has an invalid prerequisite.")
        tasks.append({
            "id": task_id,
            "title": title,
            "week": week_number,
            "order": order_number,
            "lesson_path": lesson_path,
            "hints_path": hints_path,
            "learner_template": template_path,
            "source_files": source_paths,
            "acceptance_kind": acceptance,
            "acceptance_fixture": fixture_path,
            "expected_test_count": test_count,
            "prerequisites": prerequisite_ids,
        })
        seen.add(task_id)

    if not tasks:
        raise ApiError(500, "The task catalog is empty.")
    return tasks


def extract_hint(content: str, level: int) -> str:
    matches = list(HINT_RE.finditer(content))
    for index, match in enumerate(matches):
        if int(match.group(1)) == level:
            end = matches[index + 1].start() if index + 1 < len(matches) else len(content)
            line_end = content.find("\n", match.end())
            if line_end < 0:
                return ""
            return content[line_end + 1:end].strip()
    raise ApiError(404, f"Hint level {level} is not available for this task.")


def parse_progress(output: str, tasks: list[dict[str, Any]]) -> dict[str, str]:
    states: dict[str, str] = {}
    for line in output.splitlines():
        match = PROGRESS_RE.match(line)
        if match:
            states[match.group("task")] = match.group("state")
    result: dict[str, str] = {}
    for task in tasks:
        state = states.get(task["id"], "UNKNOWN")
        result[task["id"]] = "AVAILABLE" if state == "NOT STARTED" else state
    return result


def bounded_output(value: str) -> str:
    encoded = value.encode("utf-8", errors="replace")
    if len(encoded) <= MAX_PROCESS_OUTPUT:
        return value
    clipped = encoded[:MAX_PROCESS_OUTPUT].decode("utf-8", errors="ignore")
    return clipped + "\n[CppAppLab Web] Output truncated at the configured limit.\n"


class CppAppLabWebServer(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(
        self,
        address: tuple[str, int],
        root: Path,
        binary: Path,
        workspace_root: Path,
        ui_root: Path,
    ):
        if address[0] not in ALLOWED_HOSTS:
            raise ValueError("CppAppLab Web Workbench may bind only to loopback addresses.")
        self.root = root.resolve()
        self.binary = binary.resolve()
        self.workspace_root = workspace_root
        self.ui_root = ui_root.resolve()
        self.csrf_token = secrets.token_urlsafe(32)
        self.child_env = os.environ.copy()
        self.child_env["CPPAPPLAB_DATA_DIR"] = str(self.root)
        self.child_env["HOME"] = str(Path(self.child_env.get("HOME", str(Path.home()))).expanduser())
        self.tasks = load_catalog(self.root)
        self.task_by_id = {task["id"]: task for task in self.tasks}
        super().__init__(address, CppAppLabRequestHandler)

    def run_cli(self, arguments: list[str], timeout: float = 20.0) -> tuple[int, str]:
        try:
            result = subprocess.run(
                [str(self.binary), *arguments],
                cwd=self.root,
                env=self.child_env,
                stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=timeout,
                check=False,
                close_fds=True,
                start_new_session=True,
            )
        except subprocess.TimeoutExpired as exc:
            output = exc.stdout or ""
            if isinstance(output, bytes):
                output = output.decode("utf-8", errors="replace")
            return 124, bounded_output(str(output) + "\n[CppAppLab Web] Operation timed out.\n")
        except OSError as exc:
            raise ApiError(503, f"Cannot execute the CppAppLab learning engine: {exc}") from exc
        return result.returncode, bounded_output(result.stdout or "")

    def workspace_for_task(self, task_id: str) -> tuple[Path, str]:
        task = self.task_by_id.get(task_id)
        if task is None:
            raise ApiError(404, "Unknown task ID.")
        exit_code, output = self.run_cli(["learn", task_id], timeout=20.0)
        if exit_code != 0:
            raise ApiError(409, output.strip() or "The task cannot be started yet.")
        match = re.search(r"^Workspace:\s*(.+)$", output, re.MULTILINE)
        if not match:
            raise ApiError(502, "The learning engine did not return a workspace path.")
        raw_workspace = Path(match.group(1).strip()).expanduser()
        if not raw_workspace.is_absolute():
            raw_workspace = self.root / raw_workspace
        allowed_root = self.workspace_root.resolve()
        try:
            workspace = raw_workspace.resolve(strict=True)
        except OSError as exc:
            raise ApiError(404, "The learner workspace is missing.") from exc
        try:
            relative_workspace = workspace.relative_to(allowed_root)
        except ValueError as exc:
            raise ApiError(
                403,
                "The saved workspace is outside the browser workspace root. "
                "Use the standard ~/cppapplab-work location or create a browser workspace.",
            ) from exc
        if not relative_workspace.parts:
            raise ApiError(400, "The learning engine returned an invalid workspace directory.")
        current = allowed_root
        for part in relative_workspace.parts:
            current = current / part
            try:
                current_info = current.lstat()
            except OSError as exc:
                raise ApiError(404, "The learner workspace is missing.") from exc
            if current.is_symlink():
                raise ApiError(400, "Workspace paths cannot contain symlinks.")
        if not current.is_dir():
            raise ApiError(400, "The learning engine returned an invalid workspace directory.")
        return current, output

    def resolve_workspace_file(self, task_id: str, relative: str) -> tuple[Path, dict[str, Any]]:
        task = self.task_by_id.get(task_id)
        if task is None:
            raise ApiError(404, "Unknown task ID.")
        try:
            normalized = safe_relative_path(relative)
        except ValueError as exc:
            raise ApiError(400, str(exc)) from exc
        if normalized not in task["source_files"]:
            raise ApiError(403, "This file is not declared as a learner source for the selected task.")
        workspace, _ = self.workspace_for_task(task_id)
        current = workspace
        parts = PurePosixPath(normalized).parts
        for part in parts[:-1]:
            current = current / part
            try:
                info = current.lstat()
            except OSError as exc:
                raise ApiError(404, "The declared source directory is missing.") from exc
            if current.is_symlink() or not current.is_dir():
                raise ApiError(400, "Source parent directories must be real directories.")
        target = current / parts[-1]
        info = _lstat_regular_file(target, "Learner source file")
        if info.st_size > MAX_SOURCE_BYTES:
            raise ApiError(413, "Learner source file exceeds the 1 MiB limit.")
        return target, task

    def progress_states(self) -> dict[str, str]:
        exit_code, output = self.run_cli(["progress"], timeout=15.0)
        if exit_code != 0:
            return {task["id"]: "UNKNOWN" for task in self.tasks}
        return parse_progress(output, self.tasks)


class CppAppLabRequestHandler(BaseHTTPRequestHandler):
    server: CppAppLabWebServer
    protocol_version = "HTTP/1.1"

    def log_message(self, format_string: str, *args: Any) -> None:
        sys.stderr.write("[CppAppLab Web] " + (format_string % args) + "\n")

    def _validate_host(self) -> None:
        host_header = self.headers.get("Host", "")
        parsed = urlsplit("//" + host_header)
        if parsed.hostname not in ALLOWED_HOSTS:
            raise ApiError(403, "Only loopback Host values are accepted.")
        try:
            port = parsed.port
        except ValueError as exc:
            raise ApiError(400, "Invalid Host port.") from exc
        if port is not None and port != self.server.server_port:
            raise ApiError(403, "Host port does not match the local service.")

    def _validate_same_origin(self) -> None:
        origin = self.headers.get("Origin")
        if not origin:
            raise ApiError(403, "A same-origin request is required.")
        parsed = urlsplit(origin)
        if parsed.scheme != "http" or parsed.hostname not in ALLOWED_HOSTS:
            raise ApiError(403, "Cross-origin requests are not allowed.")
        try:
            port = parsed.port or 80
        except ValueError as exc:
            raise ApiError(403, "Invalid Origin port.") from exc
        if port != self.server.server_port:
            raise ApiError(403, "Cross-origin requests are not allowed.")

    def _validate_mutation(self) -> None:
        self._validate_host()
        self._validate_same_origin()
        supplied = self.headers.get("X-CppAppLab-Token", "")
        if not secrets.compare_digest(supplied, self.server.csrf_token):
            raise ApiError(403, "The session token is missing or invalid.")

    def _send_bytes(self, status: int, content_type: str, body: bytes, head_only: bool = False) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "no-referrer")
        self.send_header(
            "Content-Security-Policy",
            "default-src 'self'; "
            "script-src 'self' https://cdn.jsdelivr.net https://gcore.jsdelivr.net "
            "https://cdnjs.cloudflare.com 'unsafe-eval'; "
            "style-src 'self' https://cdn.jsdelivr.net https://gcore.jsdelivr.net "
            "https://cdnjs.cloudflare.com 'unsafe-inline'; "
            "connect-src 'self' https://cdn.jsdelivr.net https://gcore.jsdelivr.net "
            "https://cdnjs.cloudflare.com; "
            "font-src 'self' https://cdn.jsdelivr.net https://gcore.jsdelivr.net "
            "https://cdnjs.cloudflare.com data:; "
            "worker-src blob: data:; img-src 'self' data:; object-src 'none'; "
            "base-uri 'none'; frame-ancestors 'none'",
        )
        self.end_headers()
        if not head_only:
            self.wfile.write(body)

    def _send_json(self, status: int, value: Any) -> None:
        body = json.dumps(value, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        self._send_bytes(status, "application/json; charset=utf-8", body)

    def _send_error_json(self, error: ApiError) -> None:
        self._send_json(error.status, {"error": error.message})

    def _read_json(self) -> dict[str, Any]:
        content_type = self.headers.get("Content-Type", "").split(";", 1)[0].strip().lower()
        if content_type != "application/json":
            raise ApiError(415, "Content-Type must be application/json.")
        raw_length = self.headers.get("Content-Length")
        if raw_length is None:
            raise ApiError(411, "Content-Length is required.")
        try:
            length = int(raw_length)
        except ValueError as exc:
            raise ApiError(400, "Invalid Content-Length.") from exc
        if length < 0 or length > MAX_REQUEST_BYTES:
            raise ApiError(413, "Request body exceeds the configured limit.")
        try:
            raw = self.rfile.read(length)
            value = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise ApiError(400, "Request body must be valid UTF-8 JSON.") from exc
        if not isinstance(value, dict):
            raise ApiError(400, "JSON request body must be an object.")
        return value

    def _serve_static(self, path: str, head_only: bool = False) -> None:
        relative = "index.html" if path in {"", "/"} else path.lstrip("/")
        try:
            normalized = safe_relative_path(relative)
        except ValueError as exc:
            raise ApiError(404, "Static resource not found.") from exc
        target = safe_repo_file(self.server.ui_root, normalized, 2 * 1024 * 1024, "Static resource")
        info = _lstat_regular_file(target, "Static resource")
        if info.st_size > 2 * 1024 * 1024:
            raise ApiError(413, "Static resource exceeds the size limit.")
        body = target.read_bytes()
        content_type = mimetypes.guess_type(target.name)[0] or "application/octet-stream"
        if content_type.startswith("text/") or content_type in {"application/javascript", "image/svg+xml"}:
            content_type += "; charset=utf-8"
        self._send_bytes(200, content_type, body, head_only=head_only)

    def do_HEAD(self) -> None:
        try:
            self._validate_host()
            parsed = urlsplit(self.path)
            if parsed.path.startswith("/api/"):
                raise ApiError(404, "API routes do not support HEAD.")
            self._serve_static(parsed.path, head_only=True)
        except ApiError as exc:
            self._send_error_json(exc)

    def do_GET(self) -> None:
        try:
            self._validate_host()
            parsed = urlsplit(self.path)
            segments = [unquote(part) for part in parsed.path.split("/") if part]
            if segments[:2] == ["api", "v1"]:
                self._handle_api_get(segments[2:])
            else:
                self._serve_static(parsed.path)
        except ApiError as exc:
            self._send_error_json(exc)
        except Exception as exc:  # Keep internal details in server logs, not in browser responses.
            sys.stderr.write(f"[CppAppLab Web] Internal GET error: {exc!r}\n")
            self._send_error_json(ApiError(500, "The local service encountered an internal error."))

    def _handle_api_get(self, parts: list[str]) -> None:
        if parts == ["session"]:
            self._send_json(200, {"csrf_token": self.server.csrf_token})
            return
        if parts == ["health"]:
            exit_code, output = self.server.run_cli(["--version"], timeout=5.0)
            self._send_json(200 if exit_code == 0 else 503, {
                "status": "ok" if exit_code == 0 else "degraded",
                "engine": output.strip(),
                "task_count": len(self.server.tasks),
            })
            return
        if parts == ["tasks"]:
            states = self.server.progress_states()
            values = []
            for task in self.server.tasks:
                values.append({
                    "id": task["id"],
                    "title": task["title"],
                    "week": task["week"],
                    "order": task["order"],
                    "prerequisites": task["prerequisites"],
                    "state": states.get(task["id"], "UNKNOWN"),
                })
            self._send_json(200, {"tasks": values, "count": len(values)})
            return
        if len(parts) == 2 and parts[0] == "tasks":
            self._get_task(parts[1])
            return
        if len(parts) == 4 and parts[0] == "tasks" and parts[2] == "hints":
            self._get_hint(parts[1], parts[3])
            return
        if len(parts) == 3 and parts[0] == "workspaces" and parts[2] == "files":
            self._get_workspace_files(parts[1])
            return
        if len(parts) >= 4 and parts[0] == "workspaces" and parts[2] == "files":
            self._get_workspace_file(parts[1], "/".join(parts[3:]))
            return
        raise ApiError(404, "API route not found.")

    def _get_task(self, task_id: str) -> None:
        task = self.server.task_by_id.get(task_id)
        if task is None:
            raise ApiError(404, "Unknown task ID.")
        lesson_path = safe_repo_file(
            self.server.root, task["lesson_path"], MAX_LESSON_BYTES, "Task lesson"
        )
        lesson = read_bounded(lesson_path, MAX_LESSON_BYTES, "Task lesson")
        states = self.server.progress_states()
        self._send_json(200, {
            "id": task["id"],
            "title": task["title"],
            "week": task["week"],
            "order": task["order"],
            "prerequisites": task["prerequisites"],
            "source_files": task["source_files"],
            "acceptance_kind": task["acceptance_kind"],
            "expected_test_count": task["expected_test_count"],
            "runtime_profile": RUNTIME_PROFILES.get(task_id),
            "state": states.get(task_id, "UNKNOWN"),
            "lesson_markdown": lesson,
        })

    def _get_hint(self, task_id: str, level_text: str) -> None:
        task = self.server.task_by_id.get(task_id)
        if task is None:
            raise ApiError(404, "Unknown task ID.")
        if level_text not in {"1", "2", "3"}:
            raise ApiError(400, "Hint level must be 1, 2, or 3.")
        hints_path = safe_repo_file(
            self.server.root, task["hints_path"], MAX_HINT_BYTES, "Task hints"
        )
        content = read_bounded(hints_path, MAX_HINT_BYTES, "Task hints")
        self._send_json(200, {
            "task_id": task_id,
            "level": int(level_text),
            "markdown": extract_hint(content, int(level_text)),
        })

    def _get_workspace_files(self, task_id: str) -> None:
        workspace, _ = self.server.workspace_for_task(task_id)
        task = self.server.task_by_id[task_id]
        files = []
        for relative in task["source_files"]:
            current = workspace
            for part in PurePosixPath(relative).parts:
                current = current / part
                try:
                    info = current.lstat()
                except OSError:
                    break
                if current.is_symlink():
                    raise ApiError(400, "Workspace source paths cannot contain symlinks.")
            else:
                if not current.is_file():
                    raise ApiError(400, "Workspace source entries must be regular files.")
                files.append({
                    "path": relative,
                    "size": info.st_size,
                    "language": "cpp" if current.suffix in {".cpp", ".cc", ".cxx"} else
                                "c" if current.suffix == ".c" else
                                "cpp" if current.suffix in {".hpp", ".hh", ".hxx", ".h"} else "text",
                })
        self._send_json(200, {"task_id": task_id, "files": files})

    def _get_workspace_file(self, task_id: str, relative: str) -> None:
        target, _ = self.server.resolve_workspace_file(task_id, relative)
        content = read_bounded(target, MAX_SOURCE_BYTES, "Learner source file")
        self._send_json(200, {"task_id": task_id, "path": relative, "content": content})

    def do_POST(self) -> None:
        try:
            self._validate_mutation()
            parsed = urlsplit(self.path)
            parts = [unquote(part) for part in parsed.path.split("/") if part]
            if len(parts) == 5 and parts[:2] == ["api", "v1"] and parts[2] == "tasks" and parts[4] == "start":
                self._start_task(parts[3])
                return
            if len(parts) == 5 and parts[:2] == ["api", "v1"] and parts[2] == "workspaces" and parts[4] in {"build", "run"}:
                self._run_workspace_action(parts[3], parts[4])
                return
            if len(parts) == 5 and parts[:2] == ["api", "v1"] and parts[2] == "workspaces" and parts[4] == "app-build":
                self._build_application(parts[3])
                return
            if len(parts) == 5 and parts[:2] == ["api", "v1"] and parts[2] == "workspaces" and parts[4] == "app-run":
                self._run_application(parts[3])
                return
            if len(parts) == 5 and parts[:2] == ["api", "v1"] and parts[2] == "workspaces" and parts[4] == "check":
                self._check_workspace(parts[3])
                return
            raise ApiError(404, "API route not found.")
        except ApiError as exc:
            self._send_error_json(exc)
        except Exception as exc:
            sys.stderr.write(f"[CppAppLab Web] Internal POST error: {exc!r}\n")
            self._send_error_json(ApiError(500, "The local service encountered an internal error."))

    def do_PUT(self) -> None:
        try:
            self._validate_mutation()
            parsed = urlsplit(self.path)
            parts = [unquote(part) for part in parsed.path.split("/") if part]
            if len(parts) >= 5 and parts[:2] == ["api", "v1"] and parts[2] == "workspaces" and parts[4] == "files":
                task_id = parts[3]
                relative = "/".join(parts[5:])
                self._save_workspace_file(task_id, relative)
                return
            raise ApiError(404, "API route not found.")
        except ApiError as exc:
            self._send_error_json(exc)
        except Exception as exc:
            sys.stderr.write(f"[CppAppLab Web] Internal PUT error: {exc!r}\n")
            self._send_error_json(ApiError(500, "The local service encountered an internal error."))

    def _start_task(self, task_id: str) -> None:
        if task_id not in self.server.task_by_id:
            raise ApiError(404, "Unknown task ID.")
        self._read_json()
        exit_code, output = self.server.run_cli(["learn", task_id], timeout=20.0)
        if exit_code != 0:
            raise ApiError(409, output.strip() or "The task cannot be started yet.")
        match = re.search(r"^Workspace:\s*(.+)$", output, re.MULTILINE)
        if not match:
            raise ApiError(502, "The learning engine did not return a workspace path.")
        workspace, _ = self.server.workspace_for_task(task_id)
        self._send_json(200, {
            "task_id": task_id,
            "workspace_ready": True,
            "resumed": "Resuming your saved workspace" in output,
            "files": self.server.task_by_id[task_id]["source_files"],
        })

    def _save_workspace_file(self, task_id: str, relative: str) -> None:
        value = self._read_json()
        content = value.get("content")
        if not isinstance(content, str):
            raise ApiError(400, "The content field must be a string.")
        if len(content.encode("utf-8")) > MAX_SOURCE_BYTES:
            raise ApiError(413, "Learner source file exceeds the 1 MiB limit.")
        target, _ = self.server.resolve_workspace_file(task_id, relative)
        parent = target.parent
        try:
            target_info = target.lstat()
        except OSError as exc:
            raise ApiError(404, "Learner source file was not found.") from exc
        if parent.is_symlink() or not parent.is_dir() or target.is_symlink() or not target.is_file():
            raise ApiError(400, "Only regular learner files in real directories can be saved.")
        temporary: Path | None = None
        try:
            fd, name = tempfile.mkstemp(prefix=".cppapplab-save-", suffix=".tmp", dir=parent)
            temporary = Path(name)
            with os.fdopen(fd, "w", encoding="utf-8", newline="") as stream:
                stream.write(content)
                stream.flush()
                os.fsync(stream.fileno())
            os.chmod(temporary, target_info.st_mode & 0o777)
            os.replace(temporary, target)
        except OSError as exc:
            raise ApiError(500, f"Could not save learner source file: {exc}") from exc
        finally:
            if temporary is not None and temporary.exists():
                try:
                    temporary.unlink()
                except OSError:
                    pass
        self._send_json(200, {"saved": True, "task_id": task_id, "path": relative})

    def _run_workspace_action(self, task_id: str, action: str) -> None:
        self._read_json()
        workspace, _ = self.server.workspace_for_task(task_id)
        timeout = 150.0 if action == "build" else 60.0
        exit_code, output = self.server.run_cli([action, str(workspace)], timeout=timeout)
        self._send_json(200, {
            "task_id": task_id,
            "action": action,
            "passed": exit_code == 0,
            "exit_code": exit_code,
            "output": bounded_output(output),
            "records_acceptance_evidence": False,
        })

    def _build_application(self, task_id: str) -> None:
        if task_id not in RUNTIME_PROFILES:
            raise ApiError(409, "This task has no browser application profile.")
        self._read_json()
        workspace, _ = self.server.workspace_for_task(task_id)
        exit_code, output = self.server.run_cli(
            ["app-build", str(workspace)], timeout=75.0
        )
        self._send_json(200, {
            "task_id": task_id,
            "exit_code": exit_code,
            "success": exit_code == 0,
            "output": output,
        })

    def _run_application(self, task_id: str) -> None:
        profile = RUNTIME_PROFILES.get(task_id)
        if profile is None:
            raise ApiError(409, "This task has no browser application profile.")
        value = self._read_json()
        action_id = value.get("action")
        values = value.get("values", [])
        if not isinstance(action_id, str) or not isinstance(values, list):
            raise ApiError(400, "Application action and values must be valid.")
        action = next((item for item in profile["actions"] if item["id"] == action_id), None)
        if action is None:
            raise ApiError(400, "This application action is not registered for the selected task.")
        fields = action["fields"]
        if len(values) != len(fields):
            raise ApiError(400, "The application action has an incorrect number of values.")

        arguments: list[str] = []
        for field, raw in zip(fields, values):
            if not isinstance(raw, str):
                raise ApiError(400, "Application values must be text.")
            field_type = field["type"]
            max_bytes = 256 * 1024 if field_type == "multiline" else 512
            if len(raw.encode("utf-8")) > max_bytes:
                raise ApiError(400, "Application input exceeds the configured size limit.")
            if field_type == "select":
                if raw not in field["options"]:
                    raise ApiError(400, "The selected application option is not allowed.")
            elif field_type == "number":
                if not re.fullmatch(r"-?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?", raw):
                    raise ApiError(400, "Numeric operands must be valid decimal numbers.")
            elif field_type == "id":
                if not re.fullmatch(r"[1-9]\d{0,19}", raw):
                    raise ApiError(400, "Record IDs must be positive integers.")
            elif field_type == "text":
                if not raw or any(character in raw for character in "\r\n\t\x00"):
                    raise ApiError(400, "Text values must be non-empty single-line text.")
            elif field_type == "multiline":
                if not raw or "\x00" in raw:
                    raise ApiError(400, "HTTP request input must be non-empty text without NUL bytes.")
            else:
                raise ApiError(500, "The registered application profile contains an unknown field type.")
            arguments.append(raw)

        workspace, _ = self.server.workspace_for_task(task_id)
        exit_code, output = self.server.run_cli(
            ["app-run", str(workspace), action_id, *arguments], timeout=20.0
        )
        self._send_json(200, {
            "task_id": task_id,
            "action": action_id,
            "exit_code": exit_code,
            "success": exit_code == 0,
            "records_evidence": False,
            "output": output,
        })

    def _check_workspace(self, task_id: str) -> None:
        self._read_json()
        workspace, _ = self.server.workspace_for_task(task_id)
        exit_code, summary = self.server.run_cli(["check", str(workspace)], timeout=180.0)
        output = summary
        state_directory = workspace / ".cppapplab"
        if state_directory.exists() or state_directory.is_symlink():
            if state_directory.is_symlink() or not state_directory.is_dir():
                raise ApiError(400, "Workspace check state must be a real directory.")
            log_path = state_directory / "last-check.log"
            if log_path.exists() or log_path.is_symlink():
                log = read_bounded(log_path, MAX_PROCESS_OUTPUT, "Acceptance log")
                output = (summary + "\n\n--- Acceptance log ---\n" + log).strip()
        self._send_json(200, {
            "task_id": task_id,
            "passed": exit_code == 0,
            "exit_code": exit_code,
            "summary": summary,
            "output": bounded_output(output),
        })


def main() -> int:
    parser = argparse.ArgumentParser(description="Start the local CppAppLab browser workbench.")
    default_root = Path(__file__).resolve().parents[2]
    parser.add_argument("--root", type=Path, default=default_root, help="CppAppLab repository/package root")
    parser.add_argument("--cppapplab", type=Path, default=None, help="CppAppLab executable path")
    parser.add_argument("--host", default="127.0.0.1", help="Loopback address only")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--workspace-root", type=Path, default=None)
    args = parser.parse_args()

    root = args.root.resolve()
    binary = args.cppapplab or root / "build" / "cppapplab"
    binary = binary.resolve()
    if args.host not in ALLOWED_HOSTS:
        parser.error("--host must be 127.0.0.1, localhost, or ::1")
    if not (1 <= args.port <= 65535):
        parser.error("--port must be between 1 and 65535")
    if not root.is_dir():
        parser.error(f"CppAppLab root does not exist: {root}")
    if binary.is_symlink() or not binary.is_file() or not os.access(binary, os.X_OK):
        parser.error(f"CppAppLab executable is missing or not executable: {binary}")

    home = Path(os.environ.get("HOME", str(Path.home()))).expanduser().resolve()
    workspace_root = args.workspace_root or home / "cppapplab-work"
    workspace_root = workspace_root.expanduser()
    if workspace_root.exists() and workspace_root.is_symlink():
        parser.error("The browser workspace root cannot be a symlink.")
    workspace_root.mkdir(parents=True, exist_ok=True)
    workspace_root = workspace_root.resolve()

    ui_root = root / "web" / "ui"
    if not (ui_root / "index.html").is_file():
        parser.error(f"Browser UI is missing: {ui_root / 'index.html'}")

    try:
        server = CppAppLabWebServer(
            (args.host, args.port), root, binary, workspace_root, ui_root
        )
    except OSError as exc:
        parser.error(f"Cannot start the local web service: {exc}")
    print(f"CppAppLab Web Workbench: http://127.0.0.1:{server.server_port}/", flush=True)
    print(f"Course tasks: {len(server.tasks)}", flush=True)
    print("Bound to loopback only. Press Ctrl+C to stop.", flush=True)
    def request_shutdown(signum: int, frame: Any) -> None:
        del signum, frame
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, request_shutdown)
    try:
        server.serve_forever(poll_interval=0.25)
    except KeyboardInterrupt:
        print("\nStopping CppAppLab Web Workbench.", flush=True)
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

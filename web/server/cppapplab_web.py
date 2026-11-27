#!/usr/bin/env python3
"""Local browser adapter for the existing CppAppLab learning engine.

The service intentionally exposes fixed learning actions rather than a shell.
It uses only the Python standard library and binds to loopback by default.
"""

from __future__ import annotations

import argparse
import hashlib
import http.client
import json
from collections import deque
from contextlib import nullcontext
from dataclasses import dataclass, field
import mimetypes
import os
import re
import select
import secrets
import signal
import socket
import stat
import subprocess
import sys
import threading
import time
import uuid
import tempfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path, PurePosixPath
from typing import Any
from urllib.parse import unquote, urlsplit

import knowledge_lab
import modern_cpp
import modern_projects
import wsl_handoff


MAX_CATALOG_BYTES = 64 * 1024
MAX_LESSON_BYTES = 64 * 1024
MAX_HINT_BYTES = 32 * 1024
MAX_SOURCE_BYTES = 1024 * 1024
CPP_HEADER_SUFFIXES = {".h", ".hpp", ".hh", ".hxx"}
MAX_REQUEST_BYTES = MAX_SOURCE_BYTES + 16 * 1024
MAX_PROCESS_OUTPUT = 256 * 1024
MAX_SERVICE_LOG_BYTES = 64 * 1024
MAX_SERVICE_SESSIONS = 3
SERVICE_IDLE_SECONDS = 5 * 60
SERVICE_MAX_LIFETIME_SECONDS = 30 * 60
TASK_ID_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{0,95}$")
PROGRESS_RE = re.compile(r"^\[(?P<state>[^\]]+)\]\s+(?P<task>[a-z0-9][a-z0-9._-]*)\s+·")
HINT_RE = re.compile(r"^## (?:Hint|提示) ([1-3])(?:\s|$)", re.MULTILINE)
ALLOWED_HOSTS = {"127.0.0.1", "localhost", "::1"}
NATIVE_WINDOWS = os.name == "nt"
LINUX_REQUIRED_STATE = "LINUX/WSL2 REQUIRED"


def _is_link(info: os.stat_result) -> bool:
    return stat.S_ISLNK(info.st_mode) or (
        NATIVE_WINDOWS and bool(getattr(info, "st_file_attributes", 0)
                                & stat.FILE_ATTRIBUTE_REPARSE_POINT)
    )


def _unsafe_regular(info: os.stat_result) -> bool:
    return _is_link(info) or not stat.S_ISREG(info.st_mode) or (
        NATIVE_WINDOWS and info.st_nlink != 1
    )


def _unsafe_native_component(part: str) -> bool:
    return (part in {".", ".."} or part.endswith((".", " "))
            or any(ord(char) < 32 or char in '<>:"|?*' for char in part)
            or bool(re.fullmatch(
                r"(?i)(CON|PRN|AUX|NUL|CONIN\$|CONOUT\$|COM[0-9¹²³]|LPT[0-9¹²³])",
                part.split(".")[0].rstrip())))


def _validate_native_path_chain(path: Path, *, allow_missing: bool = False) -> None:
    # Do this before resolving aliases. Junctions must not disappear from
    # validation by being resolved first. This is an ordinary local-app guard,
    # not protection against an adversary concurrently modifying the filesystem.
    if not NATIVE_WINDOWS:
        return
    path = path.absolute()
    if not re.fullmatch(r"[A-Za-z]:", path.drive) or not path.is_absolute():
        raise ApiError(400, "Native Windows workspaces need local drive paths.")
    current = Path(path.anchor)
    for part in path.parts[1:]:
        if _unsafe_native_component(part):
            raise ApiError(400, "Unsafe native Windows path component.")
        current /= part
        try:
            info = current.lstat()
        except FileNotFoundError as exc:
            if allow_missing:
                return
            raise ApiError(404, "Native Windows path was not found.") from exc
        except OSError as exc:
            raise ApiError(400, "Native Windows path could not be inspected.") from exc
        if _is_link(info):
            raise ApiError(400, "Native Windows paths cannot contain reparse points.")
        if current != path and not stat.S_ISDIR(info.st_mode):
            raise ApiError(400, "Native Windows path contains a non-directory parent.")
        if current == path and stat.S_ISREG(info.st_mode) and info.st_nlink != 1:
            raise ApiError(400, "Native Windows regular files cannot be multiply linked.")


class NativeCliSupervisor:
    """Track the real native engine; its Win32 runner owns descendant Job Objects.

    Closing/killing the CLI closes those non-inherited job handles. This is
    deliberately limited to the existing engine, never arbitrary browser commands.
    """
    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.active: set[subprocess.Popen[str]] = set()
        self.closing = False

    def run(self, command: list[str], cwd: Path, env: dict[str, str],
            timeout: float, input_data: str | None) -> tuple[int, str]:
        with self.lock:
            if self.closing:
                raise ApiError(503, "The native learning service is stopping.")
            process = subprocess.Popen(
                command, cwd=cwd, env=env,
                stdin=subprocess.DEVNULL if input_data is None else subprocess.PIPE,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, encoding="utf-8", errors="replace", close_fds=True,
                creationflags=subprocess.CREATE_NO_WINDOW,
            )
            self.active.add(process)
        try:
            try:
                output, _ = process.communicate(input=input_data, timeout=timeout)
            except subprocess.TimeoutExpired:
                process.kill()
                output, _ = process.communicate(timeout=10)
                return 124, (output or "") + "\n[CppAppLab Web] Operation timed out.\n"
            return process.returncode, output or ""
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=10)
            with self.lock:
                self.active.discard(process)

    def close(self) -> None:
        with self.lock:
            self.closing = True
            processes = tuple(self.active)
        for process in processes:
            if process.poll() is None:
                try:
                    process.kill()
                except OSError:
                    pass
        for process in processes:
            try:
                process.wait(timeout=10)
            except (OSError, subprocess.TimeoutExpired):
                sys.stderr.write("[CppAppLab Web] Native CLI did not exit on shutdown.\n")

RUNTIME_PROFILES: dict[str, dict[str, Any]] = {
    "w1.calculator": {
        "label": "命令行计算器",
        "actions": [{
            "id": "calculate", "label": "执行计算",
            "fields": [
                {"name": "operation", "label": "运算", "type": "select",
                 "options": ["add", "subtract", "multiply", "divide"],
                 "option_labels": {
                     "add": "加法（add）", "subtract": "减法（subtract）",
                     "multiply": "乘法（multiply）", "divide": "除法（divide）",
                 }},
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

SERVICE_PROFILES: dict[str, dict[str, Any]] = {
    "w7.tcp-echo": {"label": "TCP Echo Server", "kind": "tcp_echo"},
    "w7.tcp-chat": {"label": "TCP Chat Server", "kind": "tcp_chat"},
    "w8.http-server": {"label": "HTTP Server", "kind": "http_server"},
}


@dataclass
class ServiceChatClient:
    client_id: str
    nickname: str
    connection: socket.socket
    buffer: str = ""
    inbox: deque[str] = field(default_factory=deque)


@dataclass
class ServiceSession:
    task_id: str
    kind: str
    workspace: Path
    process: subprocess.Popen[str]
    port: int
    started_at: float = field(default_factory=time.monotonic)
    last_activity: float = field(default_factory=time.monotonic)
    state: str = "STARTING"
    exit_code: int | None = None
    logs: deque[str] = field(default_factory=deque)
    log_bytes: int = 0
    log_lock: threading.Lock = field(default_factory=threading.Lock)
    log_thread: threading.Thread | None = None
    clients: dict[str, "ServiceChatClient"] = field(default_factory=dict)



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
    if NATIVE_WINDOWS and any(_unsafe_native_component(part) for part in path.parts):
        raise ValueError("Unsafe Windows path component.")
    normalized = path.as_posix()
    if normalized != value:
        raise ValueError("Path must use its normalized POSIX representation.")
    return normalized


def _lstat_regular_file(path: Path, label: str) -> os.stat_result:
    try:
        info = path.lstat()
    except OSError as exc:
        raise ApiError(404, f"{label} was not found.") from exc
    if _unsafe_regular(info):
        raise ApiError(400, f"{label} must be a regular non-symlink file.")
    return info


def _safe_repo_path(root: Path, relative: str, label: str) -> tuple[Path, os.stat_result]:
    try:
        normalized = safe_relative_path(relative)
    except ValueError as exc:
        raise ApiError(400, str(exc)) from exc

    target = root / normalized
    current = root
    for part in PurePosixPath(normalized).parts:
        current = current / part
        try:
            info = current.lstat()
        except OSError as exc:
            raise ApiError(404, f"{label} was not found.") from exc
        if _is_link(info):
            raise ApiError(400, f"{label} cannot contain symlinks.")
        if current != target and not stat.S_ISDIR(info.st_mode):
            raise ApiError(400, f"{label} contains a non-directory parent.")
    return target, info


def safe_repo_file(root: Path, relative: str, max_bytes: int, label: str) -> Path:
    target, info = _safe_repo_path(root, relative, label)
    if _unsafe_regular(info):
        raise ApiError(400, f"{label} must be a regular file.")
    if info.st_size > max_bytes:
        raise ApiError(413, f"{label} exceeds its size limit.")
    return target


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


def course_translations(root: Path, relative: str, max_bytes: int, label: str) -> dict[str, str]:
    """Read bundled documents; only a missing optional sibling permits fallback."""
    original = safe_repo_file(root, relative, max_bytes, label)
    translations = {"en": read_bounded(original, max_bytes, label)}
    translated_relative = PurePosixPath(relative).with_suffix(".zh-CN.md").as_posix()
    try:
        translated = safe_repo_file(root, translated_relative, max_bytes, label)
    except ApiError as exc:
        if exc.status == 404:
            return translations
        raise
    translations["zh-CN"] = read_bounded(translated, max_bytes, label)
    return translations


def course_titles(task: dict[str, Any], translations: dict[str, str]) -> dict[str, str]:
    titles = {"en": task["title"]}
    heading = re.search(r"^#\s+(.+)$", translations.get("zh-CN", ""), re.MULTILINE)
    if heading:
        titles["zh-CN"] = heading.group(1).strip()
    return titles


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

def ensure_workspace_directory(root: Path, *parts: str) -> Path:
    current = root
    for part in parts:
        if not part or part in {".", ".."} or "/" in part or "\\" in part:
            raise ApiError(400, "Workspace runtime directory contains an unsafe component.")
        current = current / part
        try:
            status = current.lstat()
        except FileNotFoundError:
            try:
                current.mkdir()
                status = current.lstat()
            except OSError as exc:
                raise ApiError(500, f"Cannot create workspace runtime directory: {exc}") from exc
        except OSError as exc:
            raise ApiError(500, f"Cannot inspect workspace runtime directory: {exc}") from exc
        if _is_link(status) or not stat.S_ISDIR(status.st_mode):
            raise ApiError(400, "Workspace runtime directories must be real directories.")
    return current


class CppAppLabWebServer(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = not NATIVE_WINDOWS

    def server_bind(self) -> None:
        if NATIVE_WINDOWS:
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        super().server_bind()

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
        _validate_native_path_chain(root)
        _validate_native_path_chain(binary)
        _validate_native_path_chain(workspace_root)
        _validate_native_path_chain(ui_root)
        self.root = root.resolve()
        self.binary = binary.resolve()
        self.workspace_root = workspace_root
        self.ui_root = ui_root.resolve()
        self.csrf_token = secrets.token_urlsafe(32)
        self.instance_id = secrets.token_hex(16)
        self.child_env = os.environ.copy()
        self.child_env["CPPAPPLAB_DATA_DIR"] = str(self.root)
        home_key = "USERPROFILE" if NATIVE_WINDOWS else "HOME"
        self.child_env[home_key] = str(Path(self.child_env.get(home_key, str(Path.home()))).expanduser())
        self.native_cli = NativeCliSupervisor() if NATIVE_WINDOWS else None
        self.native_api_lock = threading.RLock()
        self.tasks = load_catalog(self.root)
        self.task_by_id = {task["id"]: task for task in self.tasks}
        self.service_sessions: dict[str, ServiceSession] = {}
        self.service_failures: dict[str, dict[str, Any]] = {}
        self.service_lock = threading.RLock()
        self.knowledge_lock = threading.Lock()
        self.service_janitor_stop = threading.Event()
        super().__init__(address, CppAppLabRequestHandler)
        self.service_janitor = threading.Thread(
            target=self._service_janitor_loop,
            name="cppapplab-service-janitor",
            daemon=True,
        )
        self.service_janitor.start()

    def task_executable(self, task: dict[str, Any]) -> bool:
        return not NATIVE_WINDOWS or task["week"] <= 4

    def require_task_environment(self, task_id: str) -> None:
        task = self.task_by_id.get(task_id)
        if task is not None and not self.task_executable(task):
            raise ApiError(409, "本课程需要 Linux/WSL2；请在 Ubuntu 中打开 CppAppLab。")

    def run_cli(
        self, arguments: list[str], timeout: float = 20.0, input_data: str | None = None
    ) -> tuple[int, str]:
        try:
            if self.native_cli is not None:
                code, output = self.native_cli.run(
                    [str(self.binary), *arguments], self.root, self.child_env, timeout, input_data
                )
                return code, bounded_output(output)
            result = subprocess.run(
                [str(self.binary), *arguments],
                cwd=self.root,
                env=self.child_env,
                stdin=subprocess.DEVNULL if input_data is None else None,
                input=input_data,
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

    def run_native_modern_step(self, phase: str, command: list[str], root: Path,
                               timeout: float, standard: int, pthread: bool) -> dict[str, Any]:
        _validate_native_path_chain(root)
        code, output = self.run_cli(
            ["--native-modern-step", str(root), phase, str(standard), "1" if pthread else "0"],
            timeout=timeout + 10.0,
        )
        header, separator, payload = output.partition("\n")
        match = re.fullmatch(r"CPPAPPLAB_NATIVE_PROBE_V1 (-?\d+) ([01]) ([01])", header)
        if code != 0 or not separator or match is None:
            return {"command": command, "exit_code": None, "output": output,
                    "timed_out": code == 124, "unavailable": True, "truncated": False}
        exit_code, timed, truncated = map(int, match.groups())
        return {"command": command, "exit_code": exit_code,
                "output": payload + ("\n[output truncated]" if truncated else ""),
                "timed_out": bool(timed), "truncated": bool(truncated),
                "unavailable": bool(truncated or exit_code in (126, 127))}

    def workspace_for_task(self, task_id: str) -> tuple[Path, str]:
        task = self.task_by_id.get(task_id)
        if task is None:
            raise ApiError(404, "Unknown task ID.")
        self.require_task_environment(task_id)
        exit_code, output = self.run_cli(["learn", task_id], timeout=20.0)
        if exit_code != 0:
            raise ApiError(409, output.strip() or "The task cannot be started yet.")
        match = re.search(r"^Workspace:\s*(.+)$", output, re.MULTILINE)
        if not match:
            raise ApiError(502, "The learning engine did not return a workspace path.")
        raw_workspace = Path(match.group(1).strip()).expanduser()
        if not raw_workspace.is_absolute():
            raw_workspace = self.root / raw_workspace
        _validate_native_path_chain(raw_workspace)
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
            if _is_link(current_info):
                raise ApiError(400, "Workspace paths cannot contain symlinks or reparse points.")
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
            if _is_link(info) or not stat.S_ISDIR(info.st_mode):
                raise ApiError(400, "Source parent directories must be real directories.")
        target = current / parts[-1]
        info = _lstat_regular_file(target, "Learner source file")
        if info.st_size > MAX_SOURCE_BYTES:
            raise ApiError(413, "Learner source file exceeds the 1 MiB limit.")
        return target, task

    def reference_header_file(self, task: dict[str, Any], relative: str) -> Path | None:
        if (not relative.startswith("include/")
                or PurePosixPath(relative).suffix not in CPP_HEADER_SUFFIXES
                or relative in task["source_files"]):
            return None
        try:
            return safe_repo_file(
                self.root, f"{task['learner_template']}/{relative}",
                MAX_SOURCE_BYTES, "Template reference header",
            )
        except ApiError:
            return None

    def reference_headers(self, task: dict[str, Any]) -> dict[str, int]:
        template = self.root / task["learner_template"]
        try:
            include, info = _safe_repo_path(
                self.root, f"{task['learner_template']}/include", "Template include directory"
            )
        except ApiError:
            return {}
        if not stat.S_ISDIR(info.st_mode):
            return {}

        headers = {}
        for directory, subdirs, filenames in os.walk(include, followlinks=False):
            if NATIVE_WINDOWS:
                subdirs[:] = [name for name in subdirs if not _is_link((Path(directory) / name).lstat())]
            for filename in filenames:
                relative = (Path(directory) / filename).relative_to(template).as_posix()
                target = self.reference_header_file(task, relative)
                if target is not None:
                    try:
                        headers[relative] = target.lstat().st_size
                    except OSError:
                        continue
        return headers

    def progress_states(self) -> dict[str, str]:
        exit_code, output = self.run_cli(["progress"], timeout=15.0)
        if exit_code != 0:
            return {task["id"]: "UNKNOWN" for task in self.tasks}
        states = parse_progress(output, self.tasks)
        for task in self.tasks:
            if not self.task_executable(task):
                states[task["id"]] = LINUX_REQUIRED_STATE
        return states

    @staticmethod
    def _append_service_log(session: ServiceSession, value: str) -> None:
        encoded = value.encode("utf-8", errors="replace")
        if len(encoded) > MAX_SERVICE_LOG_BYTES:
            encoded = encoded[-MAX_SERVICE_LOG_BYTES:]
            value = encoded.decode("utf-8", errors="ignore")
            encoded = value.encode("utf-8")
        with session.log_lock:
            session.logs.append(value)
            session.log_bytes += len(encoded)
            while session.log_bytes > MAX_SERVICE_LOG_BYTES and session.logs:
                removed = session.logs.popleft()
                session.log_bytes -= len(removed.encode("utf-8", errors="replace"))

    @staticmethod
    def _read_service_output(session: ServiceSession) -> None:
        stream = session.process.stdout
        if stream is None:
            return
        try:
            binary_stream = stream.buffer
            while True:
                chunk = binary_stream.read1(4096)
                if not chunk:
                    break
                CppAppLabWebServer._append_service_log(
                    session, chunk.decode("utf-8", errors="replace")
                )
        finally:
            try:
                stream.close()
            except OSError:
                pass

    @staticmethod
    def _terminate_service_process_group(session: ServiceSession, grace_seconds: float = 2.0) -> bool:
        process_group = session.process.pid
        try:
            os.killpg(process_group, signal.SIGTERM)
        except ProcessLookupError:
            if session.process.poll() is None:
                try:
                    session.process.wait(timeout=grace_seconds)
                except subprocess.TimeoutExpired:
                    pass
            return session.process.poll() is not None

        deadline = time.monotonic() + grace_seconds
        while time.monotonic() < deadline:
            if session.process.poll() is None:
                try:
                    session.process.wait(timeout=0.05)
                except subprocess.TimeoutExpired:
                    pass
            try:
                os.killpg(process_group, 0)
            except ProcessLookupError:
                break
            time.sleep(0.05)

        try:
            os.killpg(process_group, signal.SIGKILL)
        except ProcessLookupError:
            pass
        if session.process.poll() is None:
            try:
                session.process.wait(timeout=1.0)
            except subprocess.TimeoutExpired:
                return False
        try:
            os.killpg(process_group, 0)
        except ProcessLookupError:
            return True
        return False

    @staticmethod
    def _refresh_service_state(session: ServiceSession) -> None:
        exit_code = session.process.poll()
        if exit_code is not None:
            if session.state in {"STARTING", "RUNNING"}:
                CppAppLabWebServer._terminate_service_process_group(session, 0.25)
            session.exit_code = exit_code
            if session.state not in {"STOPPED", "EXPIRED"}:
                session.state = "STOPPED" if exit_code == 0 else "FAILED"

    @staticmethod
    def _service_snapshot(session: ServiceSession | None, task_id: str) -> dict[str, Any]:
        if session is None:
            return {"task_id": task_id, "state": "STOPPED", "port": None, "exit_code": None, "logs": ""}
        CppAppLabWebServer._refresh_service_state(session)
        with session.log_lock:
            logs = "".join(session.logs)
        return {
            "task_id": session.task_id,
            "kind": session.kind,
            "state": session.state,
            "port": session.port,
            "exit_code": session.exit_code,
            "started_seconds_ago": max(0, int(time.monotonic() - session.started_at)),
            "clients_count": len(session.clients),
            "logs": logs,
        }

    def service_status(self, task_id: str) -> dict[str, Any]:
        self.require_task_environment(task_id)
        if task_id not in SERVICE_PROFILES:
            raise ApiError(409, "This task has no registered network-service profile.")
        with self.service_lock:
            session = self.service_sessions.get(task_id)
            if session is None and task_id in self.service_failures:
                return dict(self.service_failures[task_id])
            return self._service_snapshot(session, task_id)

    def start_service(self, task_id: str) -> dict[str, Any]:
        self.require_task_environment(task_id)
        profile = SERVICE_PROFILES.get(task_id)
        if profile is None:
            raise ApiError(409, "This task has no registered network-service profile.")

        with self.service_lock:
            self.service_failures.pop(task_id, None)
            existing = self.service_sessions.get(task_id)
            if existing is not None:
                self._refresh_service_state(existing)
                if existing.state in {"STARTING", "RUNNING"} and existing.process.poll() is None:
                    return self._service_snapshot(existing, task_id)
                self.service_sessions.pop(task_id, None)

            active = 0
            for session in self.service_sessions.values():
                self._refresh_service_state(session)
                if session.state in {"STARTING", "RUNNING"} and session.process.poll() is None:
                    active += 1
            if active >= MAX_SERVICE_SESSIONS:
                raise ApiError(429, "The local service session limit has been reached. Stop another service first.")

            workspace, _ = self.workspace_for_task(task_id)
            build_exit, build_output = self.run_cli(["app-build", str(workspace)], timeout=75.0)
            if build_exit != 0:
                failure = {
                    "task_id": task_id,
                    "kind": profile["kind"],
                    "state": "BUILD_FAILED",
                    "port": None,
                    "exit_code": build_exit,
                    "clients_count": 0,
                    "logs": bounded_output(build_output),
                }
                self.service_failures[task_id] = failure
                return dict(failure)

            runtime_directory = ensure_workspace_directory(
                workspace, ".cppapplab", "application", "runtime"
            )
            # All registered services request a kernel-assigned ephemeral port
            # and report the actual bound port through their readiness line.
            port = 0
            executable = workspace / ".cppapplab" / "application" / "learner_app"
            try:
                executable_status = executable.lstat()
            except OSError as exc:
                message = "The learner service executable is missing. Build the application first."
                self.service_failures[task_id] = {
                    "task_id": task_id, "kind": profile["kind"], "state": "FAILED",
                    "port": None, "exit_code": None, "clients_count": 0,
                    "logs": bounded_output(build_output + "\n[CppAppLab Web] " + message + "\n"),
                }
                raise ApiError(409, message) from exc
            if executable.is_symlink() or not executable.is_file() or not os.access(executable, os.X_OK):
                message = "The learner service executable is missing or unsafe."
                self.service_failures[task_id] = {
                    "task_id": task_id, "kind": profile["kind"], "state": "FAILED",
                    "port": None, "exit_code": None, "clients_count": 0,
                    "logs": bounded_output(build_output + "\n[CppAppLab Web] " + message + "\n"),
                }
                raise ApiError(409, message)
            command = [str(executable), "server", str(port)]
            if profile["kind"] == "http_server":
                document_root = ensure_workspace_directory(
                    runtime_directory, "www"
                )
                sample_index = document_root / "index.html"
                if sample_index.exists() or sample_index.is_symlink():
                    if sample_index.is_symlink() or not sample_index.is_file():
                        raise ApiError(400, "The HTTP service document-root sample is unsafe.")
                else:
                    try:
                        sample_index.write_text(
                            "CppAppLab local document root\n", encoding="utf-8"
                        )
                    except OSError as exc:
                        raise ApiError(500, f"Cannot create HTTP service sample: {exc}") from exc
                command.append(str(document_root))

            try:
                process = subprocess.Popen(
                    command,
                    cwd=runtime_directory,
                    env=self.child_env,
                    stdin=subprocess.DEVNULL,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True,
                    encoding="utf-8",
                    errors="replace",
                    bufsize=1,
                    close_fds=True,
                    start_new_session=True,
                )
            except OSError as exc:
                message = f"Cannot start the registered learner service: {exc}"
                self.service_failures[task_id] = {
                    "task_id": task_id, "kind": profile["kind"], "state": "FAILED",
                    "port": None, "exit_code": None, "clients_count": 0,
                    "logs": bounded_output(build_output + "\n[CppAppLab Web] " + message + "\n"),
                }
                raise ApiError(503, message) from exc

            self.service_failures.pop(task_id, None)
            session = ServiceSession(
                task_id=task_id,
                kind=profile["kind"],
                workspace=workspace,
                process=process,
                port=port,
            )
            self._append_service_log(session, build_output)
            self.service_sessions[task_id] = session
            reader = threading.Thread(
                target=self._read_service_output,
                args=(session,),
                name=f"cppapplab-{profile['kind']}-logs",
                daemon=True,
            )
            session.log_thread = reader
            reader.start()
            readiness_pattern = r"listening on 127\.0\.0\.1:(\d+)"
            if profile["kind"] == "http_server":
                readiness_pattern = r"listening on (\d+)"
            deadline = time.monotonic() + 3.0
            ready = False
            while time.monotonic() < deadline:
                self._refresh_service_state(session)
                with session.log_lock:
                    log_text = "".join(session.logs)
                match = re.search(readiness_pattern, log_text)
                if match is not None:
                    reported_port = int(match.group(1))
                    if 1 <= reported_port <= 65535:
                        session.port = reported_port
                        ready = True
                        break
                if session.process.poll() is not None:
                    break
                time.sleep(0.05)

            if ready and session.process.poll() is None:
                session.state = "RUNNING"
            else:
                self._append_service_log(
                    session,
                    "\n[CppAppLab Web] Service did not report loopback readiness within 3 seconds.\n",
                )
                self._terminate_service_process_group(session, 1.0)
                session.exit_code = session.process.poll()
                session.state = "FAILED"
                if session.log_thread is not None:
                    session.log_thread.join(timeout=0.5)
            return self._service_snapshot(session, task_id)

    def send_tcp_echo(self, task_id: str, message: str) -> dict[str, Any]:
        profile = SERVICE_PROFILES.get(task_id)
        if profile is None or profile["kind"] != "tcp_echo":
            raise ApiError(409, "This task does not expose a TCP Echo client.")
        if not isinstance(message, str) or not message:
            raise ApiError(400, "Echo message must be non-empty text.")
        encoded = message.encode("utf-8")
        if len(encoded) > 1024 or any(character in message for character in "\r\n\t\x00"):
            raise ApiError(400, "Echo message must be at most 1024 bytes on one line.")

        with self.service_lock:
            session = self.service_sessions.get(task_id)
            if session is None:
                raise ApiError(409, "Start the TCP Echo service before sending a message.")
            self._refresh_service_state(session)
            if session.state != "RUNNING" or session.process.poll() is not None:
                raise ApiError(409, "The TCP Echo service is not running.")
            session.last_activity = time.monotonic()
            workspace = session.workspace
            port = session.port

        exit_code, output = self.run_cli(
            ["app-run", str(workspace), "client", str(port), message],
            timeout=10.0,
        )
        with self.service_lock:
            self._refresh_service_state(session)
            session.last_activity = time.monotonic()
            snapshot = self._service_snapshot(session, task_id)
        return {
            "task_id": task_id,
            "success": exit_code == 0,
            "exit_code": exit_code,
            "output": bounded_output(output),
            "service": snapshot,
        }

    @staticmethod
    def _collect_chat_messages(session: ServiceSession, timeout: float = 0.15) -> dict[str, list[str]]:
        deadline = time.monotonic() + max(0.0, timeout)
        while session.clients:
            connections = [client.connection for client in session.clients.values()]
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            try:
                readable, _, _ = select.select(connections, [], [], remaining)
            except (OSError, ValueError):
                break
            if not readable:
                break
            disconnected: list[str] = []
            for connection in readable:
                client = next(
                    (item for item in session.clients.values() if item.connection is connection),
                    None,
                )
                if client is None:
                    continue
                try:
                    data = connection.recv(4096)
                except (BlockingIOError, InterruptedError):
                    continue
                except OSError:
                    data = b""
                if not data:
                    disconnected.append(client.client_id)
                    continue
                client.buffer += data.decode("utf-8", errors="replace")
                if len(client.buffer.encode("utf-8", errors="replace")) > 4096:
                    disconnected.append(client.client_id)
                    continue
                while "\n" in client.buffer:
                    line, client.buffer = client.buffer.split("\n", 1)
                    if line.endswith("\r"):
                        line = line[:-1]
                    if line:
                        client.inbox.append(line)
                        while len(client.inbox) > 100:
                            client.inbox.popleft()
            for client_id in disconnected:
                stale = session.clients.pop(client_id, None)
                if stale is not None:
                    try:
                        stale.connection.close()
                    except OSError:
                        pass

        result: dict[str, list[str]] = {}
        for client_id, client in session.clients.items():
            if client.inbox:
                result[client_id] = list(client.inbox)
                client.inbox.clear()
        return result

    def connect_chat_client(self, task_id: str, nickname: str) -> dict[str, Any]:
        profile = SERVICE_PROFILES.get(task_id)
        if profile is None or profile["kind"] != "tcp_chat":
            raise ApiError(409, "This task does not expose a TCP Chat client.")
        if not isinstance(nickname, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,24}", nickname):
            raise ApiError(400, "Nickname must be 1–24 ASCII letters, digits, '_' or '-'.")
        with self.service_lock:
            session = self.service_sessions.get(task_id)
            if session is None:
                raise ApiError(409, "Start the TCP Chat service before connecting a client.")
            self._refresh_service_state(session)
            if session.state != "RUNNING" or session.process.poll() is not None:
                raise ApiError(409, "The TCP Chat service is not running.")
            if len(session.clients) >= 8:
                raise ApiError(429, "The browser workbench supports at most eight Chat clients.")
            port = session.port
            session.last_activity = time.monotonic()

        try:
            connection = socket.create_connection(("127.0.0.1", port), timeout=2.0)
            connection.settimeout(1.0)
            connection.sendall(("NICK " + nickname + "\n").encode("ascii"))
            # Keep the bounded timeout mode: select() handles reads, while
            # sendall() remains safe for the bounded 1 KiB client messages.
        except OSError as exc:
            raise ApiError(502, f"Could not connect the local Chat client: {exc}") from exc

        client_id = uuid.uuid4().hex[:12]
        client = ServiceChatClient(client_id, nickname, connection)
        with self.service_lock:
            if self.service_sessions.get(task_id) is not session or session.process.poll() is not None:
                connection.close()
                raise ApiError(409, "The TCP Chat service stopped while the client was connecting.")
            session.clients[client_id] = client
            session.last_activity = time.monotonic()
            messages = self._collect_chat_messages(session, 0.25)
            own_messages = messages.get(client_id, [])
            if not any(message == "OK NICK " + nickname for message in own_messages):
                session.clients.pop(client_id, None)
                connection.close()
                raise ApiError(502, "The learner Chat server did not acknowledge the nickname.")
            return {
                "task_id": task_id,
                "client_id": client_id,
                "nickname": nickname,
                "messages": messages,
                "service": self._service_snapshot(session, task_id),
            }

    def send_chat_message(self, task_id: str, client_id: str, message: str) -> dict[str, Any]:
        profile = SERVICE_PROFILES.get(task_id)
        if profile is None or profile["kind"] != "tcp_chat":
            raise ApiError(409, "This task does not expose a TCP Chat client.")
        if not isinstance(message, str) or not message or len(message.encode("utf-8")) > 1024:
            raise ApiError(400, "Chat message must contain 1–1024 UTF-8 bytes.")
        if any(character in message for character in "\r\n\t\x00"):
            raise ApiError(400, "Chat message must be a single line.")
        with self.service_lock:
            session = self.service_sessions.get(task_id)
            client = session.clients.get(client_id) if session is not None else None
            if session is None or client is None:
                raise ApiError(404, "Chat client was not found; connect it again.")
            self._refresh_service_state(session)
            if session.state != "RUNNING" or session.process.poll() is not None:
                raise ApiError(409, "The TCP Chat service is not running.")
            try:
                client.connection.sendall(("MSG " + message + "\n").encode("utf-8"))
            except OSError as exc:
                raise ApiError(502, f"Could not send the Chat message: {exc}") from exc
            session.last_activity = time.monotonic()
            messages = self._collect_chat_messages(session, 0.2)
            return {
                "task_id": task_id,
                "client_id": client_id,
                "messages": messages,
                "service": self._service_snapshot(session, task_id),
            }

    def poll_chat_messages(self, task_id: str, client_id: str) -> dict[str, Any]:
        profile = SERVICE_PROFILES.get(task_id)
        if profile is None or profile["kind"] != "tcp_chat":
            raise ApiError(409, "This task does not expose a TCP Chat client.")
        with self.service_lock:
            session = self.service_sessions.get(task_id)
            if session is None or client_id not in session.clients:
                raise ApiError(404, "Chat client was not found; connect it again.")
            self._refresh_service_state(session)
            session.last_activity = time.monotonic()
            messages = self._collect_chat_messages(session, 0.1)
            return {
                "task_id": task_id,
                "client_id": client_id,
                "messages": messages,
                "service": self._service_snapshot(session, task_id),
            }

    def disconnect_chat_client(self, task_id: str, client_id: str) -> dict[str, Any]:
        with self.service_lock:
            session = self.service_sessions.get(task_id)
            if session is None:
                raise ApiError(404, "Chat service session was not found.")
            client = session.clients.pop(client_id, None)
            if client is None:
                raise ApiError(404, "Chat client was not found.")
            try:
                client.connection.sendall(b"QUIT\n")
            except OSError:
                pass
            try:
                client.connection.close()
            except OSError:
                pass
            session.last_activity = time.monotonic()
            messages = self._collect_chat_messages(session, 0.1)
            return {
                "task_id": task_id,
                "client_id": client_id,
                "messages": messages,
                "service": self._service_snapshot(session, task_id),
            }

    def proxy_http_request(
        self, task_id: str, method: str, target: str, body: str = ""
    ) -> dict[str, Any]:
        profile = SERVICE_PROFILES.get(task_id)
        if profile is None or profile["kind"] != "http_server":
            raise ApiError(409, "This task does not expose an HTTP request proxy.")
        if method not in {"GET", "HEAD", "POST"}:
            raise ApiError(400, "HTTP method must be GET, HEAD or POST.")
        if not isinstance(target, str) or not target.startswith("/") or len(target) > 2048:
            raise ApiError(400, "HTTP target must be a relative path beginning with '/'.")
        parsed = urlsplit(target)
        if parsed.scheme or parsed.netloc or parsed.fragment or target.startswith("//"):
            raise ApiError(400, "Absolute URLs, authorities and fragments are not allowed.")
        decoded_target = unquote(target)
        decoded_path = unquote(parsed.path)
        if "\\" in decoded_path or any(part in {".", ".."} for part in decoded_path.split("/")):
            raise ApiError(400, "HTTP target contains an unsafe path component.")
        if any(ord(character) < 0x20 or ord(character) == 0x7F for character in decoded_target):
            raise ApiError(400, "HTTP target contains invalid control characters.")
        if not isinstance(body, str) or len(body.encode("utf-8")) > 64 * 1024:
            raise ApiError(413, "HTTP request body exceeds the 64 KiB limit.")
        if method in {"GET", "HEAD"} and body:
            raise ApiError(400, "Only POST requests may include a request body.")

        with self.service_lock:
            session = self.service_sessions.get(task_id)
            if session is None:
                raise ApiError(409, "Start the HTTP Server before sending a request.")
            self._refresh_service_state(session)
            if session.state != "RUNNING" or session.process.poll() is not None:
                raise ApiError(409, "The HTTP Server is not running.")
            port = session.port
            session.last_activity = time.monotonic()

        connection = http.client.HTTPConnection("127.0.0.1", port, timeout=3.0)
        try:
            body_bytes = body.encode("utf-8")
            headers = {
                "Host": f"127.0.0.1:{port}",
                "Connection": "close",
                "Content-Length": str(len(body_bytes)),
            }
            if method == "POST":
                headers["Content-Type"] = "text/plain; charset=utf-8"
            connection.request(method, target, body=body_bytes if method != "HEAD" else None, headers=headers)
            response = connection.getresponse()
            response_body = response.read(256 * 1024 + 1)
            if len(response_body) > 256 * 1024:
                raise ApiError(413, "HTTP response body exceeds the 256 KiB limit.")
            safe_headers = {
                name.lower(): value
                for name, value in response.getheaders()
                if name.lower() in {"content-type", "content-length", "cache-control"}
            }
            with self.service_lock:
                session.last_activity = time.monotonic()
                self._refresh_service_state(session)
                snapshot = self._service_snapshot(session, task_id)
            return {
                "task_id": task_id,
                "status_code": response.status,
                "reason": response.reason,
                "headers": safe_headers,
                "body": response_body.decode("utf-8", errors="replace"),
                "body_bytes": len(response_body),
                "service": snapshot,
            }
        except ApiError:
            raise
        except (OSError, http.client.HTTPException) as exc:
            raise ApiError(502, f"Could not proxy the request to the learner HTTP Server: {exc}") from exc
        finally:
            connection.close()

    def stop_service(self, task_id: str, reason: str = "Stopped by learner") -> dict[str, Any]:
        if task_id not in SERVICE_PROFILES:
            raise ApiError(409, "This task has no registered network-service profile.")
        with self.service_lock:
            session = self.service_sessions.get(task_id)
            if session is None:
                failure = self.service_failures.get(task_id)
                return dict(failure) if failure is not None else self._service_snapshot(None, task_id)
            session.state = "STOPPING"
            for client in list(session.clients.values()):
                try:
                    client.connection.sendall(b"QUIT\n")
                except OSError:
                    pass
                try:
                    client.connection.close()
                except OSError:
                    pass
            session.clients.clear()
            self._append_service_log(session, f"\n[CppAppLab Web] {reason}.\n")
            stopped = self._terminate_service_process_group(session, 2.0)
            if not stopped:
                self._append_service_log(
                    session, "\n[CppAppLab Web] Service process group did not exit after SIGKILL.\n"
                )
            session.exit_code = session.process.poll()
            session.state = "STOPPED" if stopped else "FAILED"
            session.last_activity = time.monotonic()
            if session.log_thread is not None:
                session.log_thread.join(timeout=0.5)
            return self._service_snapshot(session, task_id)

    def stop_all_services(self) -> None:
        with self.service_lock:
            task_ids = list(self.service_sessions)
        for task_id in task_ids:
            try:
                self.stop_service(task_id, "Web workbench shutting down")
            except ApiError as exc:
                sys.stderr.write(f"[CppAppLab Web] Could not stop {task_id}: {exc.message}\n")

    def _service_janitor_loop(self) -> None:
        while not self.service_janitor_stop.wait(15.0):
            now = time.monotonic()
            expired: list[str] = []
            with self.service_lock:
                for task_id, session in self.service_sessions.items():
                    self._refresh_service_state(session)
                    if session.state not in {"STARTING", "RUNNING"} or session.process.poll() is not None:
                        continue
                    idle = now - session.last_activity >= SERVICE_IDLE_SECONDS
                    lifetime = now - session.started_at >= SERVICE_MAX_LIFETIME_SECONDS
                    if idle or lifetime:
                        session.state = "EXPIRED"
                        expired.append(task_id)
            for task_id in expired:
                try:
                    self.stop_service(task_id, "Service session expired")
                    with self.service_lock:
                        session = self.service_sessions.get(task_id)
                        if session is not None:
                            session.state = "EXPIRED"
                except ApiError as exc:
                    sys.stderr.write(f"[CppAppLab Web] Could not expire {task_id}: {exc.message}\n")

    def server_close(self) -> None:
        if self.native_cli is not None:
            self.native_cli.close()
        self.service_janitor_stop.set()
        self.stop_all_services()
        if hasattr(self, "service_janitor"):
            self.service_janitor.join(timeout=1.0)
        super().server_close()


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
                with self.server.native_api_lock if NATIVE_WINDOWS else nullcontext():
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
        if parts == ["learning-guide"]:
            guide = safe_repo_file(
                self.server.root, "course/LEARNING_GUIDE_ZH.md", MAX_LESSON_BYTES, "Learning guide"
            )
            self._send_json(200, {"markdown": read_bounded(guide, MAX_LESSON_BYTES, "Learning guide")})
            return
        if parts == ["knowledge"]:
            topics = knowledge_lab.catalog(set(self.server.task_by_id))
            if NATIVE_WINDOWS:
                available = modern_cpp.native_compiler() is not None
                topics = [
                    dict(topic, toolchain_present=available, execution_available=available,
                         compiler_profile="Windows 随包 GCC 16.2.0",
                         environment_reason="需要已验证的 Windows 免安装包工具链。")
                    if topic["id"] in modern_cpp.BY_ID else
                    dict(topic, execution_available=False,
                         environment_reason="Windows 原生经典知识实验尚待验证；可阅读知识说明。")
                    for topic in topics
                ]
            self._send_json(200, {"topics": topics})
            return
        if parts == ["modern-projects"]:
            projects = modern_projects.catalog()
            if NATIVE_WINDOWS:
                available = modern_projects.toolchain_present()
                projects = [dict(project, execution_available=available,
                                 compiler_profile="Windows 随包 GCC 16.2.0",
                                 environment_reason="需要已验证的 Windows 免安装包工具链。")
                            for project in projects]
            self._send_json(200, {"projects": projects})
            return
        if parts == ["health"]:
            exit_code, output = self.server.run_cli(["--version"], timeout=5.0)
            self._send_json(200 if exit_code == 0 else 503, {
                "status": "ok" if exit_code == 0 else "degraded",
                "service": "CppAppLab",
                "instance_id": self.server.instance_id,
                "catalog_sha256": hashlib.sha256(
                    (self.server.root / "course/tasks.tsv").read_bytes().replace(b"\r\n", b"\n")
                ).hexdigest(),
                "engine": output.strip(),
                "task_count": len(self.server.tasks),
                "execution_environment": "native-windows" if NATIVE_WINDOWS else "linux",
                "native_course_count": sum(self.server.task_executable(task) for task in self.server.tasks),
            })
            return
        if parts == ["tasks"]:
            states = self.server.progress_states()
            values = []
            for task in self.server.tasks:
                try:
                    translations = course_translations(
                        self.server.root, task["lesson_path"], MAX_LESSON_BYTES, "Task lesson"
                    )
                except ApiError:
                    # One damaged document must not hide the catalog. Opening
                    # that lesson still enforces and reports the file guards.
                    translations = {}
                values.append({
                    "id": task["id"],
                    "title": task["title"],
                    "title_translations": course_titles(task, translations),
                    "week": task["week"],
                    "order": task["order"],
                    "prerequisites": task["prerequisites"],
                    "state": states.get(task["id"], "UNKNOWN"),
                    "execution_available": self.server.task_executable(task),
                })
            self._send_json(200, {"tasks": values, "count": len(values)})
            return
        if len(parts) == 2 and parts[0] == "tasks":
            self._get_task(parts[1])
            return
        if len(parts) == 3 and parts[0] == "tasks" and parts[2] == "prerequisite-plan":
            if NATIVE_WINDOWS:
                raise ApiError(409, "Prerequisite revalidation must use the Linux execution environment.")
            try:
                plan = wsl_handoff.prerequisite_plan(
                    self.server.task_by_id, self.server.progress_states(), parts[1])
            except wsl_handoff.HandoffError as exc:
                raise ApiError(exc.status, str(exc)) from exc
            self._send_json(200, dict(plan, execution_environment="linux",
                instance_id=self.server.instance_id,
                catalog_sha256=hashlib.sha256(
                    (self.server.root / "course/tasks.tsv").read_bytes().replace(b"\r\n", b"\n")
                ).hexdigest()))
            return
        if len(parts) == 3 and parts[0] == "tasks" and parts[2] == "history":
            self._get_task_history(parts[1])
            return
        if len(parts) == 4 and parts[0] == "tasks" and parts[2] == "hints":
            self._get_hint(parts[1], parts[3])
            return
        if len(parts) >= 2 and parts[0] == "workspaces":
            self.server.require_task_environment(parts[1])
        if len(parts) == 3 and parts[0] == "workspaces" and parts[2] == "files":
            self._get_workspace_files(parts[1])
            return
        if len(parts) == 4 and parts[0] == "workspaces" and parts[2] == "service":
            if parts[3] == "status":
                self._send_json(200, self.server.service_status(parts[1]))
                return
            if parts[3] == "logs":
                self._send_json(200, self.server.service_status(parts[1]))
                return
        if len(parts) >= 4 and parts[0] == "workspaces" and parts[2] == "files":
            self._get_workspace_file(parts[1], "/".join(parts[3:]))
            return
        raise ApiError(404, "API route not found.")

    def _get_task(self, task_id: str) -> None:
        task = self.server.task_by_id.get(task_id)
        if task is None:
            raise ApiError(404, "Unknown task ID.")
        translations = course_translations(
            self.server.root, task["lesson_path"], MAX_LESSON_BYTES, "Task lesson"
        )
        states = self.server.progress_states()
        current_state = states.get(task_id, "UNKNOWN")
        state_reason = ""
        if not self.server.task_executable(task):
            current_state = LINUX_REQUIRED_STATE
            state_reason = "本课程需要 Linux/WSL2。当前可以阅读教材与提示，请在 Ubuntu 中完成编译和验收。"
        elif current_state == "STALE":
            state_reason = "此前通过的源码指纹与当前工作区不再一致，或原工作区已不可用；请重新检查当前源码。"
        elif current_state == "NEEDS WORK":
            state_reason = "最近一次正式验收未通过；查看验收历史和诊断日志后修复并重新验收。"
        elif current_state == "LOCKED":
            missing = next((item for item in task["prerequisites"] if states.get(item) != "PASSED"), "")
            state_reason = f"需要先通过前置任务 {missing} 的当前源码验收。" if missing else "前置任务尚未全部通过。"
        self._send_json(200, {
            "id": task["id"],
            "title": task["title"],
            "title_translations": course_titles(task, translations),
            "week": task["week"],
            "order": task["order"],
            "prerequisites": task["prerequisites"],
            "source_files": task["source_files"],
            "acceptance_kind": task["acceptance_kind"],
            "expected_test_count": task["expected_test_count"],
            "runtime_profile": RUNTIME_PROFILES.get(task_id) if self.server.task_executable(task) else None,
            "service_profile": SERVICE_PROFILES.get(task_id) if self.server.task_executable(task) else None,
            "execution_available": self.server.task_executable(task),
            "state": current_state,
            "state_reason": state_reason,
            "lesson_markdown": translations["en"],
            "lesson_translations": translations,
        })

    def _get_task_history(self, task_id: str) -> None:
        if task_id not in self.server.task_by_id:
            raise ApiError(404, "Unknown task ID.")
        exit_code, output = self.server.run_cli(["history", task_id], timeout=10.0)
        if exit_code != 0:
            raise ApiError(503, "Course history is temporarily unavailable.")
        records: list[dict[str, str]] = []
        current: dict[str, str] | None = None
        for line in output.splitlines():
            stripped = line.strip()
            if not stripped or stripped.startswith("Check history:") or stripped.startswith("No check history"):
                continue
            if "  PASS  " in line or "  FAIL  " in line:
                parts = stripped.split()
                if len(parts) >= 3:
                    current = {
                        "timestamp": parts[0],
                        "result": parts[1],
                        "tests": parts[2],
                        "workspace": "",
                        "fingerprint": "",
                    }
                    records.append(current)
            elif current is not None and stripped.startswith("Workspace:"):
                current["workspace"] = stripped.partition(":")[2].strip()
            elif current is not None and stripped.startswith("Fingerprint:"):
                current["fingerprint"] = stripped.partition(":")[2].strip()
        progress = self.server.progress_states()
        self._send_json(200, {
            "task_id": task_id,
            "records": records,
            "count": len(records),
            "current_state": progress.get(task_id, "UNKNOWN"),
        })

    def _get_hint(self, task_id: str, level_text: str) -> None:
        task = self.server.task_by_id.get(task_id)
        if task is None:
            raise ApiError(404, "Unknown task ID.")
        if level_text not in {"1", "2", "3"}:
            raise ApiError(400, "Hint level must be 1, 2, or 3.")
        documents = course_translations(
            self.server.root, task["hints_path"], MAX_HINT_BYTES, "Task hints"
        )
        translations = {"en": extract_hint(documents["en"], int(level_text))}
        if "zh-CN" in documents:
            try:
                translations["zh-CN"] = extract_hint(documents["zh-CN"], int(level_text))
            except ApiError as exc:
                if exc.status != 404:
                    raise
                # An incomplete optional translation falls back one level at
                # a time, without hiding the original or disclosing later hints.
        self._send_json(200, {
            "task_id": task_id,
            "level": int(level_text),
            "markdown": translations["en"],
            "translations": translations,
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
                if _is_link(info):
                    raise ApiError(400, "Workspace source paths cannot contain symlinks or reparse points.")
            else:
                if _unsafe_regular(info):
                    raise ApiError(400, "Workspace source entries must be safe regular files.")
                files.append({
                    "path": relative,
                    "size": info.st_size,
                    "language": "cpp" if current.suffix in {".cpp", ".cc", ".cxx"} else
                                "c" if current.suffix == ".c" else
                                "cpp" if current.suffix in CPP_HEADER_SUFFIXES else "text",
                    "read_only": False,
                })
        files.extend(
            {"path": relative, "size": size, "language": "cpp", "read_only": True}
            for relative, size in sorted(self.server.reference_headers(task).items())
        )
        self._send_json(200, {"task_id": task_id, "files": files})

    def _get_workspace_file(self, task_id: str, relative: str) -> None:
        task = self.server.task_by_id.get(task_id)
        if task is None:
            raise ApiError(404, "Unknown task ID.")
        target = self.server.reference_header_file(task, relative)
        read_only = target is not None
        if target is None:
            target, _ = self.server.resolve_workspace_file(task_id, relative)
        label = "Template reference header" if read_only else "Learner source file"
        content = read_bounded(target, MAX_SOURCE_BYTES, label)
        self._send_json(200, {
            "task_id": task_id, "path": relative, "content": content, "read_only": read_only,
        })

    def do_POST(self) -> None:
        try:
            self._validate_mutation()
            with self.server.native_api_lock if NATIVE_WINDOWS else nullcontext():
                parsed = urlsplit(self.path)
                parts = [unquote(part) for part in parsed.path.split("/") if part]
                if len(parts) >= 4 and parts[:3] == ["api", "v1", "workspaces"]:
                    self.server.require_task_environment(parts[3])
                if len(parts) == 5 and parts[:3] == ["api", "v1", "tasks"] and parts[4] == "handoff":
                    task = self.server.task_by_id.get(parts[3])
                    if task is None:
                        raise ApiError(404, "Unknown task ID.")
                    if self._read_json() != {}:
                        raise ApiError(400, "Only the fixed task handoff action is accepted.")
                    if not NATIVE_WINDOWS or self.server.task_executable(task):
                        raise ApiError(409, "Only Linux-specialized tasks can be handed off from native Windows.")
                    catalog = hashlib.sha256(
                        (self.server.root / "course/tasks.tsv").read_bytes().replace(b"\r\n", b"\n")
                    ).hexdigest()
                    try:
                        result = wsl_handoff.handoff(parts[3], self.server.server_address[1], catalog)
                    except wsl_handoff.HandoffError as exc:
                        raise ApiError(exc.status, str(exc)) from exc
                    self._send_json(200, result)
                    return
                if len(parts) == 5 and parts[:3] == ["api", "v1", "modern-projects"]:
                    self._modern_project_action(parts[3], parts[4])
                    return
                if len(parts) == 5 and parts[:3] == ["api", "v1", "knowledge"] and parts[4] == "run":
                    if NATIVE_WINDOWS and (parts[3] not in modern_cpp.BY_ID or
                                           modern_cpp.native_compiler() is None):
                        raise ApiError(409, "当前 Windows 环境未开放此固定知识实验。")
                    topic = knowledge_lab.ALL_BY_ID.get(parts[3])
                    if not topic or not topic["phase"]:
                        raise ApiError(404, "Unknown runnable knowledge experiment.")
                    value = self._read_json()
                    if set(value) != {"variant"} or value["variant"] not in ("failure", "repair"):
                        raise ApiError(400, "Only the fixed failure/repair variant is accepted.")
                    if not self.server.knowledge_lock.acquire(blocking=False):
                        raise ApiError(409, "A knowledge experiment is already running.")
                    try:
                        result = (modern_cpp.run(parts[3], value["variant"], knowledge_lab.execute,
                                                 native_step=self.server.run_native_modern_step)
                                  if NATIVE_WINDOWS else knowledge_lab.run(parts[3], value["variant"]))
                        self._send_json(200, result)
                    finally:
                        self.server.knowledge_lock.release()
                    return
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
                if len(parts) == 6 and parts[:2] == ["api", "v1"] and parts[2] == "workspaces" and parts[4:] == ["service", "start"]:
                    self._read_json()
                    self._send_json(200, self.server.start_service(parts[3]))
                    return
                if len(parts) == 6 and parts[:2] == ["api", "v1"] and parts[2] == "workspaces" and parts[4:] == ["service", "stop"]:
                    self._read_json()
                    self._send_json(200, self.server.stop_service(parts[3]))
                    return
                if len(parts) == 6 and parts[:2] == ["api", "v1"] and parts[2] == "workspaces" and parts[4:] == ["service", "echo"]:
                    value = self._read_json()
                    self._send_json(200, self.server.send_tcp_echo(parts[3], value.get("message")))
                    return
                if len(parts) == 7 and parts[:2] == ["api", "v1"] and parts[2] == "workspaces" and parts[4:6] == ["service", "chat"]:
                    value = self._read_json()
                    action = parts[6]
                    if action == "connect":
                        result = self.server.connect_chat_client(parts[3], value.get("nickname"))
                    elif action == "send":
                        result = self.server.send_chat_message(
                            parts[3], value.get("client_id"), value.get("message")
                        )
                    elif action == "poll":
                        result = self.server.poll_chat_messages(parts[3], value.get("client_id"))
                    elif action == "disconnect":
                        result = self.server.disconnect_chat_client(parts[3], value.get("client_id"))
                    else:
                        raise ApiError(404, "Chat service action not found.")
                    self._send_json(200, result)
                    return
                if len(parts) == 6 and parts[:2] == ["api", "v1"] and parts[2] == "workspaces" and parts[4:] == ["service", "http"]:
                    value = self._read_json()
                    result = self.server.proxy_http_request(
                        parts[3],
                        value.get("method"),
                        value.get("target"),
                        value.get("body", ""),
                    )
                    self._send_json(200, result)
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
            with self.server.native_api_lock if NATIVE_WINDOWS else nullcontext():
                parsed = urlsplit(self.path)
                parts = [unquote(part) for part in parsed.path.split("/") if part]
                if len(parts) >= 4 and parts[:3] == ["api", "v1", "workspaces"]:
                    self.server.require_task_environment(parts[3])
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

    def _modern_project_action(self, project_id: str, action: str) -> None:
        if NATIVE_WINDOWS and not modern_projects.toolchain_present():
            raise ApiError(409, "需要已验证的 Windows 免安装包工具链。")
        if project_id not in modern_projects.PROJECTS or action not in {"start", "save", "check"}:
            raise ApiError(404, "Unknown modern project or action.")
        value = self._read_json()
        expected = {"start": set(), "save": {"source", "revision"}, "check": {"revision"}}[action]
        if set(value) != expected:
            raise ApiError(400, "Only the fixed modern project action fields are accepted.")
        if not self.server.knowledge_lock.acquire(blocking=False):
            raise ApiError(409, "Another independent experiment or modern project action is running.")
        try:
            root, workspace = self.server.root, self.server.workspace_root
            if action == "start":
                result = modern_projects.start(root, workspace, project_id)
            elif action == "save":
                result = modern_projects.save(root, workspace, project_id, value["source"], value["revision"])
            else:
                result = modern_projects.check(root, workspace, project_id, value["revision"],
                    native_step=self.server.run_native_modern_step if NATIVE_WINDOWS else None)
            self._send_json(200, result)
        except modern_projects.ProjectError as exc:
            raise ApiError(exc.status, str(exc)) from exc
        finally:
            self.server.knowledge_lock.release()

    def _start_task(self, task_id: str) -> None:
        if task_id not in self.server.task_by_id:
            raise ApiError(404, "Unknown task ID.")
        self._read_json()
        self.server.require_task_environment(task_id)
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
        if task_id in SERVICE_PROFILES:
            self.server.stop_service(task_id, "Learner source changed")
        parent = target.parent
        try:
            target_info = target.lstat()
        except OSError as exc:
            raise ApiError(404, "Learner source file was not found.") from exc
        _validate_native_path_chain(parent)
        if _is_link(parent.lstat()) or not parent.is_dir() or _unsafe_regular(target_info):
            raise ApiError(400, "Only regular learner files in real directories can be saved.")
        temporary: Path | None = None
        try:
            fd, name = tempfile.mkstemp(prefix=".cppapplab-save-", suffix=".tmp", dir=parent)
            temporary = Path(name)
            with os.fdopen(fd, "w", encoding="utf-8", newline="") as stream:
                stream.write(content)
                stream.flush()
                os.fsync(stream.fileno())
            if not NATIVE_WINDOWS:
                os.chmod(temporary, target_info.st_mode & 0o777)
            _validate_native_path_chain(target)
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
        if task_id not in RUNTIME_PROFILES and task_id not in SERVICE_PROFILES:
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
        if any(field["type"] == "multiline" for field in fields):
            # HTTP request bodies are passed over stdin instead of argv: Linux
            # limits a single argv entry and also exposes arguments in ps output.
            exit_code, output = self.server.run_cli(
                ["app-run-stdin", str(workspace), action_id],
                timeout=20.0,
                input_data=arguments[0],
            )
        else:
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
            if _is_link(state_directory.lstat()) or not state_directory.is_dir():
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


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Start the local CppAppLab browser workbench.")
    default_root = Path(__file__).resolve().parents[2]
    parser.add_argument("--root", type=Path, default=default_root, help="CppAppLab repository/package root")
    parser.add_argument("--cppapplab", type=Path, default=None, help="CppAppLab executable path")
    parser.add_argument("--host", default="127.0.0.1", help="Loopback address only")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--workspace-root", type=Path, default=None)
    parser.add_argument("--open-browser", action="store_true", help="Open the local URL after successful bind")
    args = parser.parse_args(argv)

    try:
        _validate_native_path_chain(args.root)
        binary = args.cppapplab or args.root / "build" / ("cppapplab.exe" if NATIVE_WINDOWS else "cppapplab")
        _validate_native_path_chain(binary)
    except ApiError as exc:
        parser.error(exc.message)
    root = args.root.resolve()
    binary = binary.resolve()
    if args.host not in ALLOWED_HOSTS:
        parser.error("--host must be 127.0.0.1, localhost, or ::1")
    if not (1 <= args.port <= 65535):
        parser.error("--port must be between 1 and 65535")
    if not root.is_dir():
        parser.error(f"CppAppLab root does not exist: {root}")
    if binary.is_symlink() or not binary.is_file() or not os.access(binary, os.X_OK):
        parser.error(f"CppAppLab executable is missing or not executable: {binary}")

    home_key = "USERPROFILE" if NATIVE_WINDOWS else "HOME"
    home = Path(os.environ.get(home_key, str(Path.home()))).expanduser()
    workspace_root = (args.workspace_root or home / "cppapplab-work").expanduser()
    if NATIVE_WINDOWS:
        try:
            _validate_native_path_chain(home, allow_missing=True)
            _validate_native_path_chain(workspace_root, allow_missing=True)
        except ApiError as exc:
            parser.error(exc.message)
        if workspace_root.absolute() != (home / "cppapplab-work").absolute():
            parser.error("Native Windows --workspace-root must match USERPROFILE/cppapplab-work.")
    if workspace_root.exists() and workspace_root.is_symlink():
        parser.error("The browser workspace root cannot be a symlink.")
    workspace_root.mkdir(parents=True, exist_ok=True)
    try:
        _validate_native_path_chain(workspace_root)
    except ApiError as exc:
        parser.error(exc.message)
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
        if args.open_browser:
            import webbrowser
            try:
                if not webbrowser.open(f"http://127.0.0.1:{server.server_port}/"):
                    print("Open the local URL above in your browser.", flush=True)
            except OSError:
                print("Open the local URL above in your browser.", flush=True)
        server.serve_forever(poll_interval=0.25)
    except KeyboardInterrupt:
        print("\nStopping CppAppLab Web Workbench.", flush=True)
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

"""Independent CP4 learner workspaces and real fixed CMake/GoogleTest checks.

Local learner code runs with the local user's privileges. This is not a
security sandbox. Only explicitly requested checks run a saved source snapshot.
"""
from __future__ import annotations

import hashlib
import json
import os
import re
import select
import signal
import stat
import subprocess
import tempfile
import time
from pathlib import Path
from typing import Any

import modern_cpp

MAX_SOURCE = 128 * 1024
MAX_LOG = 64 * 1024
PROJECTS = {
    "sample-window": {
        "id": "sample-window",
        "title": "C++20 · 采样窗口统计",
        "standard": 20,
        "topics": [
            "cpp20-span",
            "cpp20-ranges"
        ],
        "count": 7,
        "zh": "实现 summarize：输入只读 double 采样窗，保留有限且在闭区间 [0,100] 的样本。没有有效样本返回 nullopt；否则返回计数、算术平均、最小值、最大值。不得修改输入。先检查空窗口、正常样本，再处理 NaN/Inf 与边界。建议使用 span 和 ranges::views::filter；测试验证行为而非强制内部语法。示例数据为合成数据，不是飞行数据。",
        "en": "Implement summarize over a read-only span. Keep finite samples in [0,100]; return nullopt when none are valid, otherwise count, arithmetic mean, minimum and maximum. Preserve the input. Try empty, nominal, NaN/Inf and boundary cases. Prefer a ranges filter; tests verify behavior, not internal syntax. Synthetic data only."
    },
    "frame-header": {
        "id": "frame-header",
        "title": "C++23 · 定宽报文头解析",
        "standard": 23,
        "topics": [
            "cpp23-expected",
            "cpp23-byteswap"
        ],
        "count": 8,
        "zh": "实现 decode_header：恰好 8 字节；byte 0=0xA5，byte 1=1，byte 2–3 为大端 payload_length（0–1024），byte 4–7 为大端 uint32 sequence。错误优先级：长度、magic、version、payload_length。使用 expected 返回具体错误，禁止未检查读取。字节移位或经明确主机字节序判断的 byteswap 均可。只解析独立头部，不解析载荷，不代表真实卫星协议。",
        "en": "Implement decode_header for exactly eight bytes: magic 0xA5, version 1, big-endian payload length 0–1024 (bytes 2–3), big-endian uint32 sequence (4–7). Error precedence: size, magic, version, length. Return std::expected with specific errors; never read unchecked offsets. Shifts or endian-aware byteswap are allowed. Synthetic header only; no real satellite protocol or payload parser."
    },
}
FIXTURE_FILES = ("CMakeLists.txt", "contract.hpp", "tests.cpp")


class ProjectError(Exception):
    def __init__(self, status: int, message: str):
        super().__init__(message)
        self.status = status


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def project(project_id: str) -> dict[str, Any]:
    if project_id not in PROJECTS:
        raise ProjectError(404, "Unknown modern project.")
    return PROJECTS[project_id]


def directory(path: Path, create: bool = False) -> Path:
    if create:
        try:
            path.mkdir()
        except FileExistsError:
            pass
    try:
        mode = path.lstat().st_mode
    except FileNotFoundError as exc:
        raise ProjectError(404, "Modern project has not been opened.") from exc
    if not stat.S_ISDIR(mode):
        raise ProjectError(400, "Modern project directories must be real directories.")
    return path


def read_file(path: Path, limit: int = MAX_SOURCE) -> bytes:
    try:
        fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
        with os.fdopen(fd, "rb") as stream:
            if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
                raise ProjectError(400, "Only regular modern project files are allowed.")
            value = stream.read(limit + 1)
    except FileNotFoundError as exc:
        raise ProjectError(404, "Modern project file is missing.") from exc
    except OSError as exc:
        raise ProjectError(400, "Modern project file could not be read safely.") from exc
    if len(value) > limit:
        raise ProjectError(413, "Modern project file exceeds its limit.")
    try:
        value.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ProjectError(400, "Modern project file must be valid UTF-8.") from exc
    return value


def fixtures(root: Path, project_id: str) -> dict[str, bytes]:
    project(project_id)
    base = directory(directory(directory(root / "course") / "modern_projects") / project_id)
    return {name: read_file(base / name) for name in (*FIXTURE_FILES, "starter.cpp")}


def fixture_digest(files: dict[str, bytes]) -> str:
    # The independent check binds source and actual fixed build/test contract.
    return digest(b"".join(name.encode() + b"\0" + digest(files[name]).encode() + b"\n"
                           for name in FIXTURE_FILES))


def workspace(root: Path, project_id: str, create: bool = False) -> Path:
    project(project_id)
    return directory(directory(directory(root) / ".modern", create) / project_id, create)


def atomic_write(path: Path, value: bytes) -> None:
    fd, name = tempfile.mkstemp(prefix=".modern-save-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(value)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)


def snapshot(root: Path, workspace_root: Path, project_id: str) -> dict[str, Any]:
    meta = project(project_id)
    files = fixtures(root, project_id)
    work = workspace(workspace_root, project_id)
    source = read_file(work / "implementation.cpp")
    revision, contract = digest(source), fixture_digest(files)
    last = None
    try:
        record = json.loads(read_file(work / "last-check.json", MAX_LOG))
        if isinstance(record, dict):
            last = record
    except (ProjectError, ValueError):
        pass
    state = "UNTESTED"
    if last:
        state = ("PASS" if last.get("passed") is True else "FAIL") if (
            last.get("revision") == revision and last.get("contract") == contract) else "STALE"
    return dict(meta, source=source.decode("utf-8"), revision=revision,
                contract=files["contract.hpp"].decode("utf-8"), contract_digest=contract,
                path=str(work / "implementation.cpp"), state=state,
                toolchain_present=modern_cpp.present())


def start(root: Path, workspace_root: Path, project_id: str) -> dict[str, Any]:
    files = fixtures(root, project_id)
    work = workspace(workspace_root, project_id, True)
    try:
        fd = os.open(work / "implementation.cpp", os.O_CREAT | os.O_EXCL | os.O_WRONLY | os.O_NOFOLLOW, 0o600)
        with os.fdopen(fd, "wb") as stream:
            stream.write(files["starter.cpp"])
    except FileExistsError:
        pass  # Resume without overwriting; snapshot rejects an unsafe file.
    return snapshot(root, workspace_root, project_id)


def save(root: Path, workspace_root: Path, project_id: str,
         source: Any, revision: Any) -> dict[str, Any]:
    if not isinstance(source, str) or not isinstance(revision, str):
        raise ProjectError(400, "Source and revision must be strings.")
    try:
        value = source.encode("utf-8")
    except UnicodeEncodeError as exc:
        raise ProjectError(400, "Source must be valid UTF-8.") from exc
    if len(value) > MAX_SOURCE:
        raise ProjectError(413, "Modern learner source exceeds 128 KiB.")
    work = workspace(workspace_root, project_id)
    path = work / "implementation.cpp"
    if digest(read_file(path)) != revision:
        raise ProjectError(409, "Source changed on disk; reopen before saving.")
    atomic_write(path, value)
    return snapshot(root, workspace_root, project_id)


def execute(command: list[str], cwd: Path, timeout: float) -> dict[str, Any]:
    """Drain a bounded log and stop the process group at the deadline."""
    env = os.environ.copy()
    env.update(LC_ALL="C", LANG="C")
    result: dict[str, Any] = {"command": command, "exit_code": None, "timed_out": False,
                              "unavailable": False, "truncated": False, "output": ""}
    try:
        process = subprocess.Popen(command, cwd=cwd, env=env, stdin=subprocess.DEVNULL,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   start_new_session=True, close_fds=True)
    except OSError as exc:
        return dict(result, unavailable=True, output=str(exc))
    assert process.stdout is not None
    log = bytearray()
    deadline = time.monotonic() + timeout
    try:
        fd = process.stdout.fileno()
        os.set_blocking(fd, False)
        eof = False
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                result["timed_out"] = True
                break
            if eof and process.poll() is not None:
                break
            if eof:
                time.sleep(min(0.02, remaining))
                continue
            if not select.select([fd], [], [], min(0.05, remaining))[0]:
                continue
            try:
                data = os.read(fd, 8192)
            except BlockingIOError:
                continue
            if not data:
                eof = True
                continue
            keep = max(0, MAX_LOG - len(log))
            log.extend(data[:keep])
            if len(data) > keep:
                result["truncated"] = True
    finally:
        # Also clean up any same-group descendants left behind on normal exit.
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        process.stdout.close()
        result["exit_code"] = process.wait(timeout=3)
    result["output"] = log.decode("utf-8", errors="replace")
    if result["truncated"]:
        result["output"] += "\n[output truncated]"
    return result


def check(root: Path, workspace_root: Path, project_id: str,
          revision: Any) -> dict[str, Any]:
    meta = project(project_id)
    files = fixtures(root, project_id)
    contract = fixture_digest(files)
    work = workspace(workspace_root, project_id)
    source = read_file(work / "implementation.cpp")
    if not isinstance(revision, str) or digest(source) != revision:
        raise ProjectError(409, "Save or reopen the current source before checking.")
    steps: list[dict[str, Any]] = []
    result: dict[str, Any] = {
        "project": project_id, "revision": revision, "contract": contract,
        "passed": False, "status": "FAIL", "expected_tests": meta["count"],
        "observed_tests": 0, "steps": steps, "classic_course_acceptance": False,
    }
    with tempfile.TemporaryDirectory(prefix="cppapplab-modern-check-") as path:
        stage = Path(path)
        def step(phase: str, command: list[str], timeout: float) -> bool:
            item = execute(command, stage, timeout)
            steps.append(dict(item, phase=phase))
            return (item["exit_code"] == 0 and not item["timed_out"]
                    and not item["unavailable"] and not item["truncated"])

        if not step("toolchain", [modern_cpp.COMPILER, "-dumpfullversion", "-dumpversion"], 5):
            return dict(result, status="TOOLCHAIN_UNAVAILABLE")
        version = steps[-1]["output"].strip()
        result["compiler_version"] = version
        if not version.startswith("13."):
            return dict(result, status="TOOLCHAIN_UNAVAILABLE")
        dialect = "-std=c++20" if meta["standard"] == 20 else "-std=c++2b"
        (stage / "feature.cpp").write_text(
            "\n".join(modern_cpp.feature_source(modern_cpp.BY_ID[key]) for key in meta["topics"]),
            encoding="utf-8")
        if not step("feature-check", [modern_cpp.COMPILER, dialect, "-fsyntax-only", "feature.cpp"], 15):
            return dict(result, status="FEATURE_UNAVAILABLE")
        for name in FIXTURE_FILES:
            (stage / name).write_bytes(files[name])
        (stage / "implementation.cpp").write_bytes(source)
        configured = step("configure", ["cmake", "-S", ".", "-B", "build",
                                        "-DCMAKE_CXX_COMPILER=" + modern_cpp.COMPILER], 25)
        built = configured and step("build", ["cmake", "--build", "build", "--parallel", "2"], 45)
        tested = built and step("test", ["ctest", "--test-dir", "build", "--no-tests=error",
                                         "--output-on-failure"], 45)
        if built:
            match = re.search(r"(\d+)% tests passed, (\d+) tests failed out of (\d+)", steps[-1]["output"])
            if match:
                result["observed_tests"] = int(match.group(3))
                result["passed"] = bool(tested and match.group(1) == "100"
                                        and match.group(2) == "0"
                                        and int(match.group(3)) == meta["count"])
    if digest(read_file(work / "implementation.cpp")) != revision or fixture_digest(fixtures(root, project_id)) != contract:
        return dict(result, passed=False, status="STALE")
    result["status"] = "PASS" if result["passed"] else "FAIL"
    # Persist only small evidence, not a certificate or classic course progress.
    record = {key: result[key] for key in ("revision", "contract", "passed", "status",
                                           "compiler_version", "expected_tests", "observed_tests")}
    atomic_write(work / "last-check.json", (json.dumps(record, ensure_ascii=False) + "\n").encode())
    return result


def catalog() -> list[dict[str, Any]]:
    return [dict(meta, toolchain_present=modern_cpp.present()) for meta in PROJECTS.values()]

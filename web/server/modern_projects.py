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
from typing import Any, Callable

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


NATIVE_WINDOWS = os.name == "nt"


def native_path_guard(path: Path, *, allow_missing: bool = False) -> None:
    # Match the ordinary local-app path policy of the native workbench.
    # Not an arbitrary same-user adversarial filesystem sandbox.
    if not NATIVE_WINDOWS:
        return
    path = path.absolute()
    if not re.fullmatch(r"[A-Za-z]:", path.drive) or not path.is_absolute():
        raise ProjectError(400, "Modern projects need a local drive path.")
    current = Path(path.anchor)
    for part in path.parts[1:]:
        if (part in {".", ".."} or part.endswith((".", " ")) or
                any(ord(ch) < 32 or ch in '<>:"|?*' for ch in part) or
                re.fullmatch(r"(?i)(CON|PRN|AUX|NUL|CONIN\$|CONOUT\$|COM[0-9¹²³]|LPT[0-9¹²³])",
                             part.split(".")[0].rstrip())):
            raise ProjectError(400, "Unsafe modern project path component.")
        current /= part
        try:
            info = current.lstat()
        except FileNotFoundError as exc:
            if allow_missing:
                return
            raise ProjectError(404, "Modern project path is missing.") from exc
        except OSError as exc:
            raise ProjectError(400, "Modern project path could not be inspected.") from exc
        if (stat.S_ISLNK(info.st_mode) or
                getattr(info, "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT or
                (current != path and not stat.S_ISDIR(info.st_mode)) or
                (stat.S_ISREG(info.st_mode) and info.st_nlink != 1)):
            raise ProjectError(400, "Modern projects reject reparse points, hard links and unsafe parents.")


def toolchain_present() -> bool:
    return (modern_cpp.native_compiler() is not None if NATIVE_WINDOWS else
            modern_cpp.present())


def compiler_profile() -> str:
    return "native-windows-bundled-gcc16" if NATIVE_WINDOWS else "linux-gcc13"


def directory(path: Path, create: bool = False) -> Path:
    native_path_guard(path, allow_missing=create)
    if create:
        try:
            path.mkdir()
        except FileExistsError:
            pass
    try:
        mode = path.lstat().st_mode
    except FileNotFoundError as exc:
        raise ProjectError(404, "Modern project has not been opened.") from exc
    native_path_guard(path)
    if not stat.S_ISDIR(mode):
        raise ProjectError(400, "Modern project directories must be real directories.")
    return path


def read_file(path: Path, limit: int = MAX_SOURCE) -> bytes:
    try:
        native_path_guard(path)
        flags = (os.O_RDONLY | os.O_BINARY if NATIVE_WINDOWS else
                 os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
        fd = os.open(path, flags)
        with os.fdopen(fd, "rb") as stream:
            info = os.fstat(stream.fileno())
            if not stat.S_ISREG(info.st_mode):
                raise ProjectError(400, "Only regular modern project files are allowed.")
            if NATIVE_WINDOWS:
                native_path_guard(path)
                if info.st_nlink != 1 or not os.path.samestat(info, path.lstat()):
                    raise ProjectError(400, "Modern project file changed during opening.")
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
    native_path_guard(path.parent)
    native_path_guard(path, allow_missing=True)
    fd, name = tempfile.mkstemp(prefix=".modern-save-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(value)
            stream.flush()
            os.fsync(stream.fileno())
        native_path_guard(path, allow_missing=True)
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
    except ProjectError as exc:
        if NATIVE_WINDOWS and exc.status != 404:
            raise
    except ValueError:
        pass
    state = "UNTESTED"
    if last:
        profile_ok = (last.get("compiler_profile", "linux-gcc13") == compiler_profile()
                      and (not NATIVE_WINDOWS or last.get("compiler_version") == modern_cpp.NATIVE_VERSION))
        state = ("PASS" if last.get("passed") is True else "FAIL") if (
            last.get("revision") == revision and last.get("contract") == contract and profile_ok) else "STALE"
    return dict(meta, source=source.decode("utf-8"), revision=revision,
                contract=files["contract.hpp"].decode("utf-8"), contract_digest=contract,
                path=str(work / "implementation.cpp"), state=state,
                toolchain_present=toolchain_present(), compiler_profile=compiler_profile())


def start(root: Path, workspace_root: Path, project_id: str) -> dict[str, Any]:
    files = fixtures(root, project_id)
    work = workspace(workspace_root, project_id, True)
    try:
        target = work / "implementation.cpp"
        native_path_guard(target, allow_missing=True)
        flags = os.O_CREAT | os.O_EXCL | os.O_WRONLY
        flags |= os.O_BINARY if NATIVE_WINDOWS else os.O_NOFOLLOW
        fd = os.open(target, flags, 0o600)
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


def check_native_cache(stage: Path, compiler: Path) -> dict[str, str]:
    # Verify what CMake really selected, not merely the requested arguments.
    cache = stage / "build/CMakeCache.txt"
    native_path_guard(cache)
    data = cache.read_text(encoding="utf-8", errors="replace")
    names = ("CMAKE_CXX_COMPILER", "CMAKE_SYSROOT", "GTest_DIR", "CMAKE_MAKE_PROGRAM")
    selected = {line.split(":", 1)[0]: Path(line.split("=", 1)[1])
                for line in data.splitlines()
                if any(line.startswith(name + ":") for name in names) and "=" in line}
    prefix = compiler.parent.parent
    if (set(selected) != set(names) or
            not os.path.samefile(selected["CMAKE_CXX_COMPILER"], compiler) or
            not os.path.samefile(selected["CMAKE_SYSROOT"], prefix) or
            not os.path.samefile(selected["CMAKE_MAKE_PROGRAM"], prefix / "bin/ninja.exe") or
            not selected["GTest_DIR"].resolve().is_relative_to(prefix.resolve()) or
            not (selected["GTest_DIR"] / "GTestConfig.cmake").is_file()):
        raise ProjectError(409, "Modern project CMake did not use the bundled native toolchain.")
    for value in selected.values():
        native_path_guard(value)
    return {key: str(value) for key, value in selected.items()}


def check(root: Path, workspace_root: Path, project_id: str,
          revision: Any, *,
          native_step: Callable[[str, list[str], Path, float, int, bool], dict[str, Any]] | None = None
          ) -> dict[str, Any]:
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
        "compiler_profile": compiler_profile(),
    }
    native = native_step is not None
    selected = modern_cpp.native_compiler() if native else None
    if NATIVE_WINDOWS and (not native or selected is None):
        return dict(result, status="TOOLCHAIN_UNAVAILABLE")
    compiler = str(selected) if selected is not None else modern_cpp.COMPILER
    prefix = selected.parent.parent if selected is not None else None
    with tempfile.TemporaryDirectory(prefix="cppapplab-modern-check-") as path:
        stage = Path(path)
        native_path_guard(stage)

        def step(phase: str, command: list[str], timeout: float) -> bool:
            item = (native_step(phase, command, stage, timeout, meta["standard"], False)
                    if native_step is not None else execute(command, stage, timeout))
            steps.append(dict(item, phase=phase))
            return (item["exit_code"] == 0 and not item["timed_out"]
                    and not item["unavailable"] and not item.get("truncated", False))

        if not step("toolchain", [compiler, "-dumpfullversion", "-dumpversion"], 5):
            return dict(result, status="TOOLCHAIN_UNAVAILABLE")
        version = steps[-1]["output"].strip()
        result["compiler_version"] = version
        if (version != modern_cpp.NATIVE_VERSION if native else not version.startswith("13.")):
            return dict(result, status="TOOLCHAIN_UNAVAILABLE")
        if native and (not step("target", [compiler, "-dumpmachine"], 5) or
                       steps[-1]["output"].strip() != "x86_64-w64-mingw32"):
            return dict(result, status="TOOLCHAIN_UNAVAILABLE")
        dialect = "-std=c++20" if meta["standard"] == 20 else ("-std=c++23" if native else "-std=c++2b")
        sysroot_flags = ["--sysroot=" + prefix.as_posix()] if prefix is not None else []
        (stage / "feature.cpp").write_text(
            "\n".join(modern_cpp.feature_source(modern_cpp.BY_ID[key]) for key in meta["topics"]),
            encoding="utf-8")
        if not step("feature-check", [compiler, *sysroot_flags, dialect, "-fsyntax-only", "feature.cpp"], 15):
            return dict(result, status="FEATURE_UNAVAILABLE")
        for name in FIXTURE_FILES:
            (stage / name).write_bytes(files[name])
        (stage / "implementation.cpp").write_bytes(source)
        if native:
            assert prefix is not None and selected is not None
            cmake, ctest = str(prefix / "bin/cmake.exe"), str(prefix / "bin/ctest.exe")
            configured = step("project-configure", [cmake, "-S", ".", "-B", "build", "-G", "Ninja",
                "-DCMAKE_CXX_COMPILER:FILEPATH=" + selected.as_posix(),
                "-DCMAKE_MAKE_PROGRAM:FILEPATH=" + (prefix / "bin/ninja.exe").as_posix(),
                "-DCMAKE_SYSROOT:PATH=" + prefix.as_posix(),
                "-DCMAKE_PREFIX_PATH:PATH=" + prefix.as_posix(),
                "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE",
                "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=FALSE",
                "-DCPPAPPLAB_NATIVE_MODERN_PROFILE=ucrt64-gcc16"], 30)
            if configured:
                result["cmake_toolchain"] = check_native_cache(stage, selected)
            built = configured and step("project-build", [cmake, "--build", "build", "--parallel", "2"], 90)
            tested = built and step("project-test", [ctest, "--test-dir", "build", "--no-tests=error",
                                                    "--output-on-failure"], 45)
        else:
            configured = step("configure", ["cmake", "-S", ".", "-B", "build",
                                            "-DCMAKE_CXX_COMPILER=" + compiler], 25)
            built = configured and step("build", ["cmake", "--build", "build", "--parallel", "2"], 45)
            tested = built and step("test", ["ctest", "--test-dir", "build", "--no-tests=error",
                                             "--output-on-failure"], 45)
        if built:
            matches = re.findall(r"^\s*(\d+)% tests passed(?:, (\d+) tests failed)? out of (\d+)\s*$",
                                 steps[-1]["output"], flags=re.MULTILINE)
            if len(matches) == 1:
                percent, failed, total = matches[0]
                result["observed_tests"] = int(total)
                result["passed"] = bool(tested and percent == "100" and failed in ("", "0")
                                        and int(total) == meta["count"])
    if digest(read_file(work / "implementation.cpp")) != revision or fixture_digest(fixtures(root, project_id)) != contract:
        return dict(result, passed=False, status="STALE")
    result["status"] = "PASS" if result["passed"] else "FAIL"
    # Persist only small evidence, not a certificate or classic course progress.
    record = {key: result[key] for key in ("revision", "contract", "passed", "status",
                                           "compiler_version", "compiler_profile",
                                           "expected_tests", "observed_tests")}
    atomic_write(work / "last-check.json", (json.dumps(record, ensure_ascii=False) + "\n").encode())
    return result


def catalog() -> list[dict[str, Any]]:
    return [dict(meta, toolchain_present=toolchain_present(), compiler_profile=compiler_profile())
            for meta in PROJECTS.values()]

"""Native Windows source-build web acceptance; actual HTTP and learning engine."""
from __future__ import annotations

import ctypes
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request


def verify(root: Path, binary: Path) -> None:
    if os.name != "nt":
        raise RuntimeError("Native Windows runner required; this is not an emulation test.")
    with tempfile.TemporaryDirectory(prefix="cppapplab-native-web-") as temporary:
        base = Path(temporary)
        home = base / "用户 配置"
        home.mkdir()
        env = os.environ.copy()
        env.update(USERPROFILE=str(home), CPPAPPLAB_STATE_DIR=str(base / "学习记录"),
                   CPPAPPLAB_NATIVE_BUILD_ROOT=str(base / "cache"))
        for key in ("CPPAPPLAB_DATA_DIR", "CPPAPPLAB_LAUNCH_COMMAND"):
            env.pop(key, None)
        server = root / "web/server/cppapplab_web.py"
        token = ""
        url = ""
        proc = None

        def request(path: str, method: str = "GET", body: dict | None = None,
                    expected: int | tuple[int, ...] = 200, authenticated: bool = True):
            headers = {}
            if method != "GET":
                headers = {"Origin": url, "Content-Type": "application/json"}
                if authenticated:
                    headers["X-CppAppLab-Token"] = token
            data = None if body is None else json.dumps(body).encode("utf-8")
            req = urllib.request.Request(url + path, data=data, headers=headers, method=method)
            try:
                response = urllib.request.urlopen(req, timeout=190)
            except urllib.error.HTTPError as error:
                response = error
            with response:
                allowed = (expected,) if isinstance(expected, int) else expected
                raw = response.read()
                assert response.status in allowed, (path, response.status, raw[:4000])
                return json.loads(raw) if response.headers.get_content_type() == "application/json" else raw

        def command(port: int) -> list[str]:
            return [sys.executable, str(server), "--root", str(root),
                    "--cppapplab", str(binary), "--host", "127.0.0.1",
                    "--port", str(port)]

        def start(log):
            nonlocal url
            with socket.socket() as reservation:
                reservation.bind(("127.0.0.1", 0))
                port = reservation.getsockname()[1]
            url = f"http://127.0.0.1:{port}"
            process = subprocess.Popen(command(port), cwd=root, env=env, stdout=log, stderr=log)
            try:
                for _ in range(150):
                    if process.poll() is not None:
                        raise RuntimeError("Native web service exited on startup")
                    try:
                        health = request("/api/v1/health")
                        assert health["execution_environment"] == "native-windows"
                        assert health["status"] == "ok" and health["task_count"] == 25
                        assert health["native_course_count"] == 5
                        return process, port
                    except (OSError, urllib.error.URLError):
                        time.sleep(0.1)
                raise RuntimeError("Native web readiness timed out")
            except BaseException:
                stop(process)
                raise

        def stop(process):
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=15)

        def states() -> dict:
            return {task["id"]: task["state"] for task in request("/api/v1/tasks")["tasks"]}

        def save(task_id: str, relative: str, content: str) -> None:
            assert request(f"/api/v1/workspaces/{task_id}/files/{relative}", "PUT",
                           {"content": content})["saved"]

        def verify_supervisor_shutdown() -> None:
            # A real learner process must be alive when we close the native
            # Python supervisor; terminating only a dummy CLI is insufficient.
            sys.path.insert(0, str(root / "web/server"))
            from cppapplab_web import NativeCliSupervisor, ApiError
            isolated = base / "shutdown-probe"
            isolated.mkdir()
            profile = isolated / "home"
            profile.mkdir()
            workspace = isolated / "workspace"
            child_env = dict(env, USERPROFILE=str(profile),
                             CPPAPPLAB_STATE_DIR=str(isolated / "state"),
                             CPPAPPLAB_DATA_DIR=str(root))
            subprocess.run([str(binary), "start", "w1.hello-world", str(workspace)],
                           env=child_env, cwd=root, check=True, capture_output=True, timeout=20)
            (workspace / "src/main.cpp").write_text(
                '#include <windows.h>\n#include <fstream>\n'
                'int main(){std::ofstream out("child.pid");out<<GetCurrentProcessId();'
                'out.close();Sleep(30000);return 0;}\n', encoding="utf-8")
            supervisor = NativeCliSupervisor()
            result = []

            def invoke():
                try:
                    result.append(supervisor.run([str(binary), "check", str(workspace)],
                                                 root, child_env, 45.0, None))
                except BaseException as error:
                    result.append(error)

            thread = threading.Thread(target=invoke)
            thread.start()
            handle = None
            kernel = ctypes.WinDLL("kernel32", use_last_error=True)
            kernel.OpenProcess.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32]
            kernel.OpenProcess.restype = ctypes.c_void_p
            kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
            kernel.WaitForSingleObject.restype = ctypes.c_uint32
            kernel.CloseHandle.argtypes = [ctypes.c_void_p]
            try:
                pid_path = workspace / "child.pid"
                deadline = time.monotonic() + 20
                while time.monotonic() < deadline and thread.is_alive():
                    if pid_path.is_file():
                        pid_text = pid_path.read_text(encoding="ascii").strip()
                        if pid_text.isdigit():
                            handle = kernel.OpenProcess(0x00100000, False, int(pid_text))
                            if handle:
                                break
                    time.sleep(0.02)
                assert handle and kernel.WaitForSingleObject(handle, 0) == 258, result
                supervisor.close()
                assert kernel.WaitForSingleObject(handle, 10000) == 0, "Orphan learner process"
                thread.join(timeout=15)
                assert not thread.is_alive() and result, result
                assert isinstance(result[0], tuple) and result[0][0] != 0, result
                try:
                    supervisor.run([str(binary), "--version"], root, child_env, 5.0, None)
                except ApiError as error:
                    assert error.status == 503
                else:
                    raise AssertionError("Supervisor accepted work after close")
                print("[PASS] Native shutdown closes real learner descendant Job Object.")
            finally:
                supervisor.close()
                thread.join(timeout=15)
                if handle:
                    kernel.CloseHandle(handle)

        log_path = base / "server.log"
        with log_path.open("w+", encoding="utf-8") as log:
            try:
                proc, port = start(log)
                token = request("/api/v1/session")["csrf_token"]
                assert b"CppAppLab" in request("/") and request("/app.js") and request("/app.css")
                catalog = request("/api/v1/tasks")["tasks"]
                assert len(catalog) == 25
                assert sum(task["execution_available"] for task in catalog) == 5
                assert sum(task["state"] == "LINUX/WSL2 REQUIRED" for task in catalog) == 20
                assert request("/api/v1/learning-guide")["markdown"]
                linux = request("/api/v1/tasks/w5.system-lab")
                assert linux["execution_available"] is False and linux["lesson_markdown"]
                assert linux["runtime_profile"] is None and linux["service_profile"] is None
                request("/api/v1/tasks/w5.system-lab/start", "POST", {}, expected=409)
                request("/api/v1/workspaces/w7.tcp-echo/service/start", "POST", {}, expected=409)
                topics = request("/api/v1/knowledge")["topics"]
                assert topics and all(topic["execution_available"] is False for topic in topics)
                topic = next(topic for topic in topics if topic["phase"])
                request(f"/api/v1/knowledge/{topic['id']}/run", "POST",
                        {"variant": "repair"}, expected=409)
                projects = request("/api/v1/modern-projects")["projects"]
                assert len(projects) == 2 and all(not p["execution_available"] for p in projects)
                request(f"/api/v1/modern-projects/{projects[0]['id']}/start", "POST", {}, expected=409)

                hello = "/api/v1/workspaces/w1.hello-world"
                assert states()["w1.hello-world"] == "AVAILABLE"
                request("/api/v1/tasks/w1.hello-world/start", "POST", {}, expected=403,
                        authenticated=False)
                assert request("/api/v1/tasks/w1.hello-world/start", "POST", {})["workspace_ready"]
                assert not request(hello + "/check", "POST", {})["passed"]
                save("w1.hello-world", "src/main.cpp",
                     '#include <iostream>\nint main(){std::cout<<"Hello, CppAppLab!\\n";}\n')
                for action in ("build", "run"):
                    result = request(hello + "/" + action, "POST", {})
                    assert result["passed"] and not result["records_acceptance_evidence"], result
                assert states()["w1.hello-world"] != "PASSED"
                assert request(hello + "/check", "POST", {})["passed"]
                assert states()["w1.hello-world"] == "PASSED"
                assert states()["w1.calculator"] == "AVAILABLE"

                # Seed the existing reference only in this test; production uses learner code.
                assert request("/api/v1/tasks/w1.calculator/start", "POST", {})["workspace_ready"]
                cal = "/api/v1/workspaces/w1.calculator"
                task = request("/api/v1/tasks/w1.calculator")
                reference = root / "labs/w1/calculator/reference"
                for relative in task["source_files"]:
                    save("w1.calculator", relative, (reference / relative).read_text(encoding="utf-8"))
                result = request(cal + "/check", "POST", {})
                assert result["passed"] and "Acceptance: 9/9" in result["summary"], result
                assert states()["w2.todo-cli"] == "AVAILABLE"

                # Actual filesystem hard-link refusal, not an emulated path.
                source = home / "cppapplab-work/w1.hello-world/src/main.cpp"
                original = source.read_bytes()
                outside = base / "outside.cpp"
                outside.write_bytes(b"unchanged")
                source.unlink()
                os.link(outside, source)
                request(hello + "/files/src/main.cpp", expected=(400, 403, 409))
                request(hello + "/files/src/main.cpp", "PUT", {"content": "bad"},
                        expected=(400, 403, 409))
                assert outside.read_bytes() == b"unchanged"
                source.unlink()
                source.write_bytes(original)

                conflict = subprocess.run(command(port), cwd=root, env=env,
                                          capture_output=True, timeout=10)
                assert conflict.returncode != 0
                launcher = subprocess.run(
                    ["pwsh", "-NoProfile", "-File", str(root / "scripts/start-native-web-workbench.ps1"),
                     "-Binary", str(binary), "-Python", sys.executable, "-Port", str(port)],
                    cwd=root, env=env, capture_output=True, timeout=20)
                assert launcher.returncode != 0, "Launcher must reject an occupied port"
                assert request("/api/v1/health")["status"] == "ok"

                stop(proc)
                proc = None
                proc, _ = start(log)
                token = request("/api/v1/session")["csrf_token"]
                assert states()["w1.hello-world"] == "PASSED"
                assert request("/api/v1/tasks/w1.hello-world/start", "POST", {})["resumed"]
                save("w1.hello-world", "src/main.cpp", "int main(){return 0;}\n")
                assert states()["w1.hello-world"] == "STALE"
                assert states()["w5.system-lab"] == "LINUX/WSL2 REQUIRED"
                verify_supervisor_shutdown()
                print("[PASS] Native Windows HTTP: five enabled tasks, W1 fail/build/run/PASS, "
                      "Calculator 9/9, restart/resume/stale, hard-link refusal, port conflict, "
                      "token and unavailable-execution guards.")
            except BaseException:
                log.flush()
                print(log_path.read_text(encoding="utf-8", errors="replace"), file=sys.stderr)
                raise
            finally:
                if proc is not None:
                    stop(proc)


if __name__ == "__main__":
    verify(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())

"""Exercise a relocated prebuilt learner package via its real HTTP service."""
from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request


def verify(package: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="cppapplab packaged web ") as temporary:
        base = Path(temporary)
        relocated = base / "learner package"
        shutil.copytree(package, relocated)
        home = base / "home"
        home.mkdir()
        env = os.environ.copy()
        for key in ("CPPAPPLAB_DATA_DIR", "CPPAPPLAB_BINARY", "CPPAPPLAB_LAUNCH_COMMAND"):
            env.pop(key, None)
        env.update(HOME=str(home), CPPAPPLAB_STATE_DIR=str(base / "state"))
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            port = reservation.getsockname()[1]
        env["CPPAPPLAB_WEB_PORT"] = str(port)
        url = f"http://127.0.0.1:{port}"
        token = ""

        def request(path: str, method: str = "GET", body: dict | None = None):
            headers = {}
            if method != "GET":
                headers = {"Origin": url, "X-CppAppLab-Token": token,
                           "Content-Type": "application/json"}
            data = None if body is None else json.dumps(body).encode("utf-8")
            req = urllib.request.Request(url + path, data=data, headers=headers, method=method)
            with urllib.request.urlopen(req, timeout=90) as response:
                raw = response.read()
                return json.loads(raw) if response.headers.get_content_type() == "application/json" else raw

        def state(task_id: str) -> str:
            return next(t["state"] for t in request("/api/v1/tasks")["tasks"] if t["id"] == task_id)

        def start(log):
            proc = subprocess.Popen([str(relocated / "start-web-workbench")],
                                    cwd=base, env=env, stdout=log, stderr=log)
            try:
                for _ in range(100):
                    if proc.poll() is not None:
                        raise RuntimeError("Packaged service exited during startup")
                    try:
                        health = request("/api/v1/health")
                        if health["service"] == "CppAppLab" and health["status"] == "ok":
                            assert health["task_count"] == 25, health
                            return proc
                    except (OSError, urllib.error.URLError):
                        pass
                    time.sleep(0.1)
                raise RuntimeError("Packaged workbench readiness timed out")
            except BaseException:
                stop(proc)
                raise

        def stop(proc):
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()
                    raise

        log_path = base / "service.log"
        proc = None
        with log_path.open("w+", encoding="utf-8") as log:
            try:
                proc = start(log)
                assert b"CppAppLab" in request("/")
                assert request("/app.js") and request("/app.css")
                assert request("/api/v1/learning-guide")["markdown"]
                assert request("/api/v1/knowledge")["topics"]
                assert len(request("/api/v1/modern-projects")["projects"]) == 2
                token = request("/api/v1/session")["csrf_token"]
                assert state("w1.hello-world") == "AVAILABLE"
                started = request("/api/v1/tasks/w1.hello-world/start", "POST", {})
                assert started["workspace_ready"] and not started["resumed"], started
                endpoint = "/api/v1/workspaces/w1.hello-world"
                rejected = request(endpoint + "/check", "POST", {})
                assert not rejected["passed"], rejected
                result = request(endpoint + "/files/src/main.cpp", "PUT",
                                 {"content": '#include <iostream>\nint main(){std::cout << "Hello, CppAppLab!\\n";}\n'})
                assert result["saved"], result
                for action in ("build", "run"):
                    result = request(endpoint + "/" + action, "POST", {})
                    assert result["passed"] and not result["records_acceptance_evidence"], result
                assert state("w1.hello-world") != "PASSED"
                result = request(endpoint + "/check", "POST", {})
                assert result["passed"], result
                assert state("w1.hello-world") == "PASSED"
                assert state("w1.calculator") == "AVAILABLE"
                stop(proc)
                proc = None
                proc = start(log)
                token = request("/api/v1/session")["csrf_token"]
                assert state("w1.hello-world") == "PASSED"
                resumed = request("/api/v1/tasks/w1.hello-world/start", "POST", {})
                assert resumed["workspace_ready"] and resumed["resumed"], resumed
                request(endpoint + "/files/src/main.cpp", "PUT",
                        {"content": "int main(){return 0;}\n"})
                assert state("w1.hello-world") == "STALE"
                assert state("w1.calculator") == "LOCKED"
                # A second instance cannot silently reuse an existing port.
                conflict = subprocess.run([str(relocated / "start-web-workbench")],
                                          cwd=base, env=env, capture_output=True,
                                          text=True, timeout=10)
                assert conflict.returncode != 0, conflict
                assert request("/api/v1/health")["status"] == "ok"
            except BaseException:
                log.flush()
                print(log_path.read_text(encoding="utf-8", errors="replace"), file=sys.stderr)
                raise
            finally:
                if proc is not None:
                    stop(proc)
        print("[PASS] Relocated packaged web service: 25 tasks, assets, guide, knowledge, "
              "modern catalog, W1 FAIL/build/run/PASS, restart/resume, STALE/relock, port conflict.")


if __name__ == "__main__":
    verify(Path(sys.argv[1]).resolve())

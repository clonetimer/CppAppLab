"""Extract a real Windows candidate, verify every byte, exercise shipped tools.

This acceptance deliberately strips all non-system PATH entries before invoking
the candidate. It uses no repo server, repo binary, host GCC or host Python
for learner operations. It is not a clean physical-machine/network-isolation test.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
import zipfile


def verify_extract(archive: Path, destination: Path) -> dict:
    with zipfile.ZipFile(archive) as z:
        manifest = json.loads(z.read("package-manifest.json"))
        assert manifest["status"] == "CI_CANDIDATE_NOT_PUBLIC_RELEASE"
        entries = [info.filename for info in z.infolist()]
        assert len({entry.casefold() for entry in entries}) == len(entries)
        assert set(entries) == set(manifest["files"]) | {"package-manifest.json"}
        for name in entries:
            rel = PurePosixPath(name)
            assert not rel.is_absolute() and rel.as_posix() == name
            assert all(part not in {"", ".", ".."} and ":" not in part
                       and "\\" not in part for part in rel.parts)
            path = destination / name
            path.parent.mkdir(parents=True, exist_ok=True)
            with z.open(name) as source, path.open("xb") as target:
                h = hashlib.sha256()
                count = 0
                while chunk := source.read(1024 * 1024):
                    h.update(chunk)
                    count += len(chunk)
                    target.write(chunk)
            if name != "package-manifest.json":
                assert manifest["files"][name] == {"sha256": h.hexdigest(), "bytes": count}, name
    print(f"[PASS] Extracted and verified {len(manifest['files'])} package files.")
    return manifest


def verify(archive: Path) -> None:
    # Keep the test harness itself capable of printing the captured UTF-8 output.
    for stream in (sys.stdout, sys.stderr):
        if stream is not None:
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    if os.name != "nt":
        raise RuntimeError("Native Windows host required.")
    expected_sha, expected_file = (archive.parent / "SHA256SUMS").read_text(
        encoding="ascii").strip().split(maxsplit=1)
    assert expected_file == archive.name
    with archive.open("rb") as stream:
        assert hashlib.file_digest(stream, "sha256").hexdigest() == expected_sha

    with tempfile.TemporaryDirectory(prefix="cppapplab-portable-") as scratch:
        base = Path(scratch)
        # Deliberately use a relocated install directory containing spaces.
        # Chinese learner and progress locations remain independent of it.
        package = base / "Relocated learner package"
        manifest = verify_extract(archive, package)
        acquisition = package / "provenance/acquisition-replay.json"
        baseline = package / "provenance/ucrt64-versions.json"
        frozen = package / "provenance/ucrt64-archives.json"
        report = json.loads(acquisition.read_text(encoding="utf-8"))
        lock = json.loads(baseline.read_text(encoding="utf-8"))
        frozen_lock = json.loads(frozen.read_text(encoding="utf-8"))
        assert hashlib.sha256(acquisition.read_bytes()).hexdigest() == manifest["acquisition_report_sha256"]
        assert hashlib.sha256(baseline.read_bytes()).hexdigest() == report["version_baseline_sha256"]
        assert hashlib.sha256(frozen.read_bytes()).hexdigest() == manifest["frozen_archive_lock_sha256"] == report["frozen_lock_sha256"]
        assert report["status"] == "FROZEN_ARCHIVE_REPLAY_VERIFIED"
        assert frozen_lock["status"] == "FROZEN_ARCHIVE_LOCK"
        assert report["review"] == frozen_lock["review"]
        assert report["packages"] == frozen_lock["packages"]
        assert report["signature_policy"] == "Required TrustedOnly; no configured repositories"
        assert {name: value["version"] for name, value in report["packages"].items()} == lock["packages"]
        assert {meta["name"][0]: meta["version"][0] for meta in manifest["toolchain_packages"]} == lock["packages"]
        print(f"[PASS] Packaged acquisition provenance: {len(lock['packages'])} exact signed archive identities and replay receipt.")
        home = base / "学员 配置"
        home.mkdir()
        system = Path(os.environ["SystemRoot"])
        env = {k: v for k, v in os.environ.items()
               if not k.upper().startswith(("CPPAPPLAB_", "PYTHON", "CMAKE_", "MSYS", "MINGW"))}
        env.update(PATH=os.pathsep.join((str(system / "System32"), str(system))),
                   USERPROFILE=str(home), CPPAPPLAB_STATE_DIR=str(base / "学习记录"),
                   CPPAPPLAB_NATIVE_BUILD_ROOT=str(base / "cache"),
                   CPPAPPLAB_NO_PAUSE="1")
        # A hostile toolchain variable must not leak through the package launcher.
        env["CMAKE_PREFIX_PATH"] = str(base / "nonexistent-host-cmake")
        env["GCC_EXEC_PREFIX"] = str(base / "nonexistent-host-gcc")
        env["CPPAPPLAB_NATIVE_SYSROOT"] = str(base / "nonexistent-host-sysroot")
        python = package / "runtime/python.exe"
        launcher = package / "start-native.py"
        cmd = system / "System32/cmd.exe"

        def batch(file: str, arguments: str, timeout: int = 90):
            # cmd /s /c removes the outer pair, preserving the quoted batch path.
            command = f'"{cmd}" /d /s /c ""{package / file}" {arguments}"'
            return subprocess.run(command, executable=str(cmd), env=env, cwd=base,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                  timeout=timeout, encoding="utf-8", errors="replace")

        versions = batch("Start-CppAppLab.cmd", "--verify-tools")
        assert versions.returncode == 0, versions.stdout
        assert "x86_64-w64-mingw32" in versions.stdout and "gdb.exe:" in versions.stdout
        assert "CppAppLab Windows 原生候选包" in versions.stdout, versions.stdout
        assert "[PASS] Relocated GCC C++17 header, link and executable probe." in versions.stdout, versions.stdout
        print(versions.stdout)
        cli = batch("cppapplab.cmd", "--version")
        assert cli.returncode == 0 and manifest["version"] in cli.stdout, cli.stdout
        embedded_version = subprocess.check_output(
            [str(python), "--version"], env=env, cwd=base, text=True, timeout=10)
        assert manifest["python"]["version"] in embedded_version, embedded_version

        token, url = "", ""
        proc = None

        def request(path, method="GET", body=None, status=200):
            headers = {}
            if method != "GET":
                headers = {"Origin": url, "X-CppAppLab-Token": token,
                           "Content-Type": "application/json"}
            data = None if body is None else json.dumps(body).encode("utf-8")
            req = urllib.request.Request(url + path, data=data, method=method, headers=headers)
            try:
                response = urllib.request.urlopen(req, timeout=190)
            except urllib.error.HTTPError as exc:
                response = exc
            with response:
                raw = response.read()
                assert response.status == status, (path, response.status, raw[:4000])
                return json.loads(raw) if response.headers.get_content_type() == "application/json" else raw

        def start(log):
            nonlocal url
            with socket.socket() as reservation:
                reservation.bind(("127.0.0.1", 0))
                port = reservation.getsockname()[1]
            url = f"http://127.0.0.1:{port}"
            process = subprocess.Popen([str(python), str(launcher), "--no-browser",
                                        "--port", str(port)],
                                       env=env, cwd=base, stdout=log, stderr=log)
            try:
                for _ in range(150):
                    if process.poll() is not None:
                        raise RuntimeError("Relocated candidate server exited during startup")
                    try:
                        health = request("/api/v1/health")
                        if health["status"] == "ok":
                            assert health["execution_environment"] == "native-windows"
                            assert health["native_course_count"] == 5 and health["task_count"] == 25
                            return process, port
                    except OSError:
                        pass
                    time.sleep(0.1)
                raise RuntimeError("Portable service readiness timed out")
            except BaseException:
                stop(process)
                raise

        def stop(process):
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=15)

        def states():
            return {task["id"]: task["state"] for task in request("/api/v1/tasks")["tasks"]}

        log_path = base / "server.log"
        with log_path.open("w+", encoding="utf-8") as log:
            try:
                proc, port = start(log)
                token = request("/api/v1/session")["csrf_token"]
                assert b"CppAppLab" in request("/") and request("/app.js") and request("/app.css")
                assert states()["w1.hello-world"] == "AVAILABLE"
                assert sum(s == "LINUX/WSL2 REQUIRED" for s in states().values()) == 20
                request("/api/v1/tasks/w5.system-lab/start", "POST", {}, status=409)
                # Guarded rejection paths must not invoke WSL on CI or on
                # native-course startup. Real WSL2 handoff is a separate gate.
                request("/api/v1/tasks/w1.hello-world/handoff", "POST", {}, status=409)
                request("/api/v1/tasks/w5.system-lab/handoff", "POST", {"command": "anything"}, status=400)
                request("/api/v1/tasks/w9.unknown/handoff", "POST", {}, status=404)
                request("/api/v1/tasks/w5.system-lab/prerequisite-plan", status=409)
                assert states()["w1.hello-world"] == "AVAILABLE"
                catalog = request("/api/v1/tasks")["tasks"]
                expected = dict(zip(manifest["formal_task_ids"], (1, 9, 19, 31, 41), strict=True))
                total = 0
                for task_id, count in expected.items():
                    assert states()[task_id] == "AVAILABLE", (task_id, states())
                    started = request(f"/api/v1/tasks/{task_id}/start", "POST", {})
                    assert started["workspace_ready"] and not started["resumed"], started
                    endpoint = f"/api/v1/workspaces/{task_id}"
                    if task_id == "w1.hello-world":
                        red = request(endpoint + "/check", "POST", {})
                        assert not red["passed"], red
                        # A broken compiler is not a legitimate wrong-output
                        # learner failure. Inspect the real fixed-check log.
                        red_log = (home / "cppapplab-work" / task_id /
                                   ".cppapplab/last-check.log").read_text(
                                       encoding="utf-8", errors="replace")
                        assert "Output did not match the task contract." in red_log, red_log
                    task = request(f"/api/v1/tasks/{task_id}")
                    # Resolve the original frozen test-only source seeding from
                    # the package's own task table and reference baselines.
                    row = next(line.split("\t") for line in
                               (package / "course/tasks.tsv").read_text(encoding="utf-8").splitlines()
                               if line.startswith(task_id + "\t"))
                    reference = (package / row[6]).parent / "reference"
                    for relative in task["source_files"]:
                        if task_id == "w1.hello-world":
                            source = '#include <iostream>\nint main(){std::cout<<"Hello, CppAppLab!\\n";}\n'
                        else:
                            ref = (reference / "src/task_manager_test.cpp"
                                   if task_id == "w4.quality-gate" and relative == "tests/task_manager_test.cpp"
                                   else reference / relative)
                            source = ref.read_text(encoding="utf-8")
                        saved = request(endpoint + "/files/" + relative, "PUT", {"content": source})
                        assert saved["saved"], saved
                    if task_id in {"w1.hello-world", "w1.calculator"}:
                        for action in ("build", "run"):
                            result = request(endpoint + "/" + action, "POST", {})
                            assert result["passed"] and not result["records_acceptance_evidence"], result
                        assert states()[task_id] != "PASSED"
                    result = request(endpoint + "/check", "POST", {})
                    assert result["passed"] and f"Acceptance: {count}/{count}" in result["summary"], result
                    assert states()[task_id] == "PASSED", states()
                    evidence = home / "cppapplab-work" / task_id / ".cppapplab/evidence.txt"
                    assert evidence.is_file()
                    total += count
                    print(f"[PASS] Relocated packaged native {task_id}: {count}/{count}.")
                assert total == 101

                # Exercise the frozen 10 modern topic pairs in the actual
                # relocated workbench using the bundled GCC, never host tools.
                state_before = states()
                history_root = base / "学习记录"
                history_before = {str(path.relative_to(history_root)): path.read_bytes()
                                  for path in history_root.rglob("*") if path.is_file()}
                topics = request("/api/v1/knowledge")["topics"]
                modern = [topic for topic in topics if topic["standard"] in (20, 23)]
                assert len(modern) == 10, modern
                assert all(topic["execution_available"] and topic["toolchain_present"]
                           for topic in modern), modern
                assert all(not topic["execution_available"] for topic in topics
                           if topic["standard"] == 17)
                for topic in modern:
                    route = "/api/v1/knowledge/" + topic["id"] + "/run"
                    request(route, "POST", {"variant": "repair", "compiler": "g++"}, status=400)
                    for variant in ("failure", "repair"):
                        result = request(route, "POST", {"variant": variant})
                        assert result["observed"] and not result["course_acceptance"], result
                        assert result["compiler_version"] == "16.2.0", result
                        assert result["compiler_profile"] == "native-windows-bundled-gcc16", result
                        steps = result["steps"]
                        assert [step["phase"] for step in steps[:3]] == ["toolchain", "target", "feature-check"], result
                        assert steps[1]["output"].strip() == "x86_64-w64-mingw32"
                        assert steps[2]["exit_code"] == 0 and not steps[2]["unavailable"]
                        assert os.path.samefile(steps[0]["command"][0], package / "toolchain/bin/g++.exe")
                        assert steps[-1]["phase"] == (topic["phase"] if variant == "failure" else "run")
                        assert steps[-1]["exit_code"] != 0 if variant == "failure" else steps[-1]["output"].strip() == "42"
                        print(f"[PASS] Packaged native modern {topic['id']} {variant}.")
                assert states() == state_before
                assert {str(path.relative_to(history_root)): path.read_bytes()
                        for path in history_root.rglob("*") if path.is_file()} == history_before
                print("[PASS] Native modern fixed experiments: 20/20; classic evidence unchanged.")

                # Existing CP4 editable projects, fixed starters and fixtures;
                # reference is supplied only by this explicit test harness.
                projects = request("/api/v1/modern-projects")["projects"]
                expected_projects = {"sample-window": 7, "frame-header": 8}
                assert {project["id"]: project["count"] for project in projects} == expected_projects
                for project in projects:
                    assert project["execution_available"] and project["toolchain_present"], project
                    project_id = project["id"]
                    count = expected_projects[project_id]
                    route = "/api/v1/modern-projects/" + project_id
                    fixture = package / "course/modern_projects" / project_id
                    opened = request(route + "/start", "POST", {})
                    assert opened["source"] == (fixture / "starter.cpp").read_text(encoding="utf-8"), opened
                    assert opened["state"] == "UNTESTED", opened
                    request(route + "/check", "POST", {"revision": opened["revision"], "compiler": "g++"}, status=400)
                    red = request(route + "/check", "POST", {"revision": opened["revision"]})
                    assert not red["passed"] and red["status"] == "FAIL", red
                    assert red["observed_tests"] == count and red["steps"][-1]["phase"] == "project-test", red
                    assert red["steps"][-1]["exit_code"] != 0 and not red["steps"][-1]["timed_out"], red
                    reference = (fixture / "reference.cpp").read_text(encoding="utf-8")
                    saved = request(route + "/save", "POST", {"source": reference, "revision": opened["revision"]})
                    assert saved["state"] == "STALE" and saved["revision"] != opened["revision"], saved
                    request(route + "/save", "POST", {"source": "", "revision": opened["revision"]}, status=409)
                    request(route + "/check", "POST", {"revision": opened["revision"]}, status=409)
                    good = request(route + "/check", "POST", {"revision": saved["revision"]})
                    assert good["passed"] and good["status"] == "PASS" and good["observed_tests"] == count, good
                    assert not good["classic_course_acceptance"] and good["compiler_version"] == "16.2.0", good
                    assert good["compiler_profile"] == "native-windows-bundled-gcc16", good
                    actual = good["cmake_toolchain"]
                    prefix = package / "toolchain"
                    assert os.path.samefile(actual["CMAKE_CXX_COMPILER"], prefix / "bin/g++.exe")
                    assert os.path.samefile(actual["CMAKE_SYSROOT"], prefix)
                    assert os.path.samefile(actual["CMAKE_MAKE_PROGRAM"], prefix / "bin/ninja.exe")
                    assert Path(actual["GTest_DIR"]).resolve().is_relative_to(prefix.resolve())
                    resumed = request(route + "/start", "POST", {})
                    assert resumed["source"] == reference and resumed["state"] == "PASS", resumed
                    changed = request(route + "/save", "POST", {"source": reference + "\n// new revision\n", "revision": saved["revision"]})
                    assert changed["state"] == "STALE", changed
                    print(f"[PASS] Packaged native editable modern {project_id}: starter FAIL, reference {count}/{count}, resume/stale/conflict.")
                assert states() == state_before
                assert {str(path.relative_to(history_root)): path.read_bytes()
                        for path in history_root.rglob("*") if path.is_file()} == history_before
                print("[PASS] Native editable modern projects: 15/15; classic evidence unchanged.")

                # At least four genuine CMake configurations must have used the
                # relocated compiler and relocated GTest config, not the host.
                caches = list((base / "cache").rglob("CMakeCache.txt"))
                assert len(caches) == 4, caches
                prefix = (package / "toolchain").resolve()
                for cache in caches:
                    data = cache.read_text(encoding="utf-8", errors="replace")
                    selected = {line.split(":", 1)[0]: Path(line.split("=", 1)[1])
                                for line in data.splitlines()
                                if line.startswith(("CMAKE_CXX_COMPILER:", "GTest_DIR:", "CMAKE_SYSROOT:"))}
                    assert set(selected) == {"CMAKE_CXX_COMPILER", "GTest_DIR", "CMAKE_SYSROOT"}, (cache, selected)
                    assert os.path.samefile(selected["CMAKE_SYSROOT"], prefix), selected
                    assert os.path.samefile(selected["CMAKE_CXX_COMPILER"], prefix / "bin/g++.exe"), selected
                    assert selected["GTest_DIR"].resolve().is_relative_to(prefix), selected
                    assert (selected["GTest_DIR"] / "GTestConfig.cmake").is_file(), selected
                conflict = batch("Start-CppAppLab.cmd", f"--no-browser --port {port}")
                assert conflict.returncode != 0, conflict.stdout
                assert request("/api/v1/health")["status"] == "ok"
                stop(proc)
                proc = None
                proc, _ = start(log)
                token = request("/api/v1/session")["csrf_token"]
                assert all(states()[task] == "PASSED" for task in expected)
                assert request("/api/v1/tasks/w1.hello-world/start", "POST", {})["resumed"]
                request("/api/v1/workspaces/w1.hello-world/files/src/main.cpp", "PUT",
                        {"content": "int main(){return 0;}\n"})
                assert states()["w1.hello-world"] == "STALE"
                print("[PASS] Portable ZIP native HTTP: 101/101 formal contracts, bundled "
                      "compiler and GTest paths, batch launch, restart/resume/stale, port conflict.")
            except BaseException:
                log.flush()
                print(log_path.read_text(encoding="utf-8", errors="replace"), file=sys.stderr)
                raise
            finally:
                if proc is not None:
                    stop(proc)


if __name__ == "__main__":
    verify(Path(sys.argv[1]).resolve())

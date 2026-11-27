"""On-demand handoff to an already prepared WSL2 CppAppLab checkout.

No setup, download, git pull, installation, source copying, or PASS migration.
No client-controlled shell command, filesystem path, host or port.
"""
from __future__ import annotations

import json
import os
from pathlib import Path
import re
import subprocess
import urllib.request
from urllib.parse import quote

TASK = re.compile(r"w[5-9]\.[a-z0-9][a-z0-9._-]{0,92}")
ANY_TASK = re.compile(r"w[1-9]\.[a-z0-9][a-z0-9._-]{0,92}")
LIVE_STATES = {"AVAILABLE", "NOT STARTED", "IN PROGRESS", "PASSED", "NEEDS WORK", "STALE", "LOCKED"}
DISTRO = re.compile(r"^\s*(\*)?\s*(Ubuntu(?:[- ](?:22\.04|24\.04))?)\s+.+?\s+([12])\s*$", re.I)
TOKEN = re.compile(r"[a-f0-9]{32}")
DIGEST = re.compile(r"[a-f0-9]{64}")
MARKER = "CPPAPPLAB_HANDOFF_V1 "
BOOTSTRAP = r'''
set -eo pipefail
task="$1"
port="$2"
. /etc/os-release
if [ "$ID" != ubuntu ] || { [ "$VERSION_ID" != 22.04 ] && [ "$VERSION_ID" != 24.04 ]; }; then
    echo "CppAppLab requires Ubuntu 22.04 or 24.04." >&2
    exit 21
fi
repo="$HOME/work/CppAppLab"
if [ ! -x "$repo/build/cppapplab" ] || [ ! -f "$repo/scripts/start-web-workbench.sh" ] || [ ! -f "$repo/course/tasks.tsv" ]; then
    echo "Prepare the Ubuntu checkout ~/work/CppAppLab and its build first; no automatic setup was performed." >&2
    exit 22
fi
remote="$(git -C "$repo" remote get-url origin 2>/dev/null | tr '[:upper:]' '[:lower:]' || true)"
case "$remote" in
    https://github.com/clonetimer/cppapplab.git|https://github.com/clonetimer/cppapplab|git@github.com:clonetimer/cppapplab.git|git@github.com:clonetimer/cppapplab|ssh://git@github.com/clonetimer/cppapplab.git) ;;
    *) echo "The prepared directory is not the expected CppAppLab checkout." >&2; exit 23 ;;
esac
unset CPPAPPLAB_STATE_DIR CPPAPPLAB_DATA_DIR CPPAPPLAB_BINARY CPPAPPLAB_LAUNCH_COMMAND CPPAPPLAB_NATIVE_SYSROOT CPPAPPLAB_NATIVE_BUILD_ROOT
cd "$repo"
CPPAPPLAB_OPEN_BROWSER=0 CPPAPPLAB_WEB_PORT="$port" /bin/bash scripts/start-web-workbench.sh start >&2
python3 - "$port" "$task" <<'PY'
import hashlib
import json
from pathlib import Path
import sys
import urllib.request
port, task = sys.argv[1:]
opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
def read(path):
    with opener.open("http://127.0.0.1:" + port + path, timeout=8) as response:
        return json.loads(response.read(512 * 1024))
health = read("/api/v1/health")
catalog = hashlib.sha256(Path("course/tasks.tsv").read_bytes().replace(b"\r\n", b"\n")).hexdigest()
if (health.get("service") != "CppAppLab" or health.get("status") != "ok" or
        health.get("execution_environment") != "linux" or
        health.get("catalog_sha256") != catalog or not health.get("instance_id")):
    raise SystemExit("The prepared Linux workbench must be updated to the handoff-compatible version.")
plan = read("/api/v1/tasks/" + task + "/prerequisite-plan")
if (plan.get("target_task") != task or plan.get("execution_environment") != "linux" or
        plan.get("instance_id") != health["instance_id"] or plan.get("catalog_sha256") != catalog):
    raise SystemExit("The Linux prerequisite plan does not match the verified workbench.")
print("CPPAPPLAB_HANDOFF_V1 " + json.dumps({
    "instance_id": health["instance_id"], "catalog_sha256": catalog,
    "task": task, "task_state": plan.get("target_state"),
    "next_task": plan.get("next_task"), "pending_prerequisites": plan.get("pending_prerequisites"),
    "execution_environment": "linux"
}, ensure_ascii=True))
PY
'''


class HandoffError(Exception):
    def __init__(self, status: int, message: str):
        super().__init__(message)
        self.status = status


def prerequisite_plan(task_by_id: dict, states: dict, target: str) -> dict:
    """Order actual Linux prerequisites by dependency, never import Windows PASS."""
    if not TASK.fullmatch(target) or target not in task_by_id:
        raise HandoffError(404, "Unknown Linux-specialized target course.")
    ordered = []
    visited = set()
    visiting = set()

    def visit(task_id):
        if task_id in visited:
            return
        if task_id in visiting or task_id not in task_by_id:
            raise HandoffError(409, "Course prerequisite catalog is inconsistent.")
        visiting.add(task_id)
        for parent in task_by_id[task_id]["prerequisites"]:
            visit(parent)
        visiting.remove(task_id)
        visited.add(task_id)
        ordered.append(task_id)

    visit(target)
    if any(states.get(task_id) not in LIVE_STATES for task_id in ordered):
        raise HandoffError(409, "Linux course progress is not available; refresh the Linux workbench.")
    pending = [{"task_id": task_id, "state": states[task_id]}
               for task_id in ordered[:-1] if states[task_id] != "PASSED"]
    next_task = pending[0]["task_id"] if pending else target
    # A locked first unmet prerequisite means the engine and graph disagree;
    # do not bypass the lock or manufacture availability.
    if states[next_task] == "LOCKED":
        raise HandoffError(409, "Linux prerequisite state is inconsistent; refresh the Linux workbench.")
    return {"target_task": target, "target_state": states[target],
            "next_task": next_task, "pending_prerequisites": pending}


def decode_output(data: bytes) -> str:
    if data.startswith((b"\xff\xfe", b"\xfe\xff")):
        return data.decode("utf-16", errors="replace").replace("\x00", "")
    if b"\x00" in data:
        return data.decode("utf-16-le", errors="replace").replace("\x00", "")
    return data.decode("utf-8", errors="replace")


def select_distro(output: str) -> str:
    entries = []
    for line in output.splitlines():
        match = DISTRO.fullmatch(line)
        if match and match.group(3) == "2":
            entries.append((bool(match.group(1)), match.group(2)))
    if not entries:
        raise HandoffError(409, "未发现受支持的 Ubuntu WSL2。请先手动准备 Ubuntu 22.04 或 24.04；本机课程不需要 WSL。")
    defaults = [name for default, name in entries if default]
    if defaults:
        return defaults[0]
    return sorted((name for _, name in entries),
                  key=lambda name: (0 if "24.04" in name else 1 if "22.04" in name else 2,
                                    name.casefold()))[0]


def fixed_run(command: list[str], timeout: int) -> bytes:
    try:
        result = subprocess.run(command, stdin=subprocess.DEVNULL,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                timeout=timeout, check=False, shell=False,
                                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    except subprocess.TimeoutExpired as exc:
        raise HandoffError(504, "WSL 响应超时；请检查其启动状态。没有自动安装或复制学习记录。") from exc
    except OSError as exc:
        raise HandoffError(409, "无法启动 WSL；请检查已安装的 Ubuntu 环境。") from exc
    if result.returncode:
        detail = decode_output(result.stdout[-4096:]).strip()
        raise HandoffError(409, "WSL 教学环境未就绪：" + (detail or "请检查 wsl -l -v。"))
    if len(result.stdout) > 512 * 1024:
        raise HandoffError(502, "WSL 返回了超出限制的状态信息。")
    return result.stdout


def windows_health(port: int) -> dict:
    try:
        opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
        with opener.open(f"http://127.0.0.1:{port}/api/v1/health", timeout=8) as response:
            data = response.read(64 * 1024 + 1)
        if len(data) > 64 * 1024:
            raise ValueError("health response too large")
        value = json.loads(data)
        if not isinstance(value, dict):
            raise ValueError("invalid health response")
        return value
    except (OSError, ValueError) as exc:
        raise HandoffError(409, "Windows 无法通过回环地址访问 WSL 工作台。请检查 WSL localhost 转发及端口冲突。") from exc


def handoff(task: str, native_port: int, catalog_sha256: str, *,
            run=fixed_run, health=windows_health, wsl: Path | None = None) -> dict:
    if not TASK.fullmatch(task) or not DIGEST.fullmatch(catalog_sha256):
        raise HandoffError(400, "仅允许已登记的 W5–W9 课程交接。")
    if wsl is None:
        if os.name != "nt":
            raise HandoffError(409, "只有 Windows 原生工作台可以发起此交接。")
        wsl = Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32/wsl.exe"
        if not wsl.is_file():
            raise HandoffError(409, "尚未安装 WSL。请手动准备 Ubuntu WSL2；本机课程不受影响。")
    distro = select_distro(decode_output(run([str(wsl), "--list", "--verbose"], 20)))
    port = 8767 if native_port == 8766 else 8766
    output = decode_output(run([str(wsl), "--distribution", distro, "--exec",
                                "/bin/bash", "-c", BOOTSTRAP, "cppapplab-handoff", task, str(port)], 65))
    records = [line[len(MARKER):] for line in output.splitlines() if line.startswith(MARKER)]
    try:
        if len(records) != 1:
            raise ValueError("missing or ambiguous handoff record")
        receipt = json.loads(records[0])
        if (receipt.get("task") != task or receipt.get("execution_environment") != "linux" or
                receipt.get("catalog_sha256") != catalog_sha256 or
                not TOKEN.fullmatch(str(receipt.get("instance_id", ""))) or
                receipt.get("task_state") not in LIVE_STATES):
            raise ValueError("invalid Linux receipt")
        pending = receipt["pending_prerequisites"]
        next_task = receipt["next_task"]
        if (not isinstance(pending, list) or len(pending) > 24 or
                any(not isinstance(row, dict) or set(row) != {"task_id", "state"} or
                    not isinstance(row["task_id"], str) or not ANY_TASK.fullmatch(row["task_id"]) or
                    row["task_id"] == task or row["state"] not in LIVE_STATES - {"PASSED"}
                    for row in pending) or
                len({row["task_id"] for row in pending}) != len(pending) or
                next_task != (pending[0]["task_id"] if pending else task) or
                (pending and pending[0]["state"] == "LOCKED") or
                (not pending and receipt["task_state"] == "LOCKED")):
            raise ValueError("invalid Linux prerequisite plan")
    except (ValueError, TypeError, AttributeError, KeyError) as exc:
        raise HandoffError(409, "WSL 工作台课程目录或实例身份不匹配；请更新已准备的 Ubuntu 工程。") from exc
    target = health(port)
    if (target.get("status") != "ok" or target.get("service") != "CppAppLab" or
            target.get("execution_environment") != "linux" or
            target.get("instance_id") != receipt["instance_id"] or
            target.get("catalog_sha256") != catalog_sha256):
        raise HandoffError(409, "Windows 回环端口上的服务不是刚核验的 WSL 工作台；不自动跳转。")
    url = f"http://127.0.0.1:{port}/#task={quote(next_task, safe='')}"
    if next_task != task:
        url += "&goal=" + quote(task, safe="")
    return {"url": url, "distribution": distro, "task_id": task,
            "task_state": receipt["task_state"], "next_task_id": next_task,
            "pending_prerequisites": pending,
            "execution_environment": "linux", "progress_transferred": False}

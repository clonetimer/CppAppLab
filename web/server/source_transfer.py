"""Explicit source-only exchange between independent CP7 execution environments.

A bundle is not an acceptance receipt, signature, or trusted executable.
Only exact declared UTF-8 learner sources can enter a newly created workspace.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import stat
import tempfile

KIND = "cppapplab-source-reuse"
MAX_TASK_BYTES = 512 * 1024
MAX_TASK_JSON = 768 * 1024
MAX_BUNDLE_JSON = 4 * 1024 * 1024
FOUNDATION_IDS = frozenset(("w1.hello-world", "w1.calculator", "w2.todo-cli",
                            "w3.task-manager", "w4.quality-gate"))


class TransferError(Exception):
    def __init__(self, status: int, message: str):
        super().__init__(message)
        self.status = status


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def encoded_size(value: dict) -> int:
    try:
        return len(json.dumps(value, ensure_ascii=False, separators=(",", ":")).encode("utf-8"))
    except (UnicodeError, ValueError, TypeError) as exc:
        raise TransferError(400, "Source exchange must be valid UTF-8 JSON.") from exc


def source_bytes(path: Path) -> bytes:
    """Read a caller-validated path; reject unsafe leaf replacement/hardlinks."""
    try:
        before = path.lstat()
        if (not stat.S_ISREG(before.st_mode) or before.st_nlink != 1 or
                getattr(before, "st_file_attributes", 0) &
                getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)):
            raise TransferError(400, "Only safe single-link regular learner sources can be exchanged.")
        flags = os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0)
        flags |= getattr(os, "O_NONBLOCK", 0)
        fd = os.open(path, flags)
        with os.fdopen(fd, "rb") as stream:
            opened = os.fstat(stream.fileno())
            if not stat.S_ISREG(opened.st_mode) or opened.st_nlink != 1 or not os.path.samestat(before, opened):
                raise TransferError(409, "Source changed while opening; refresh and retry.")
            data = stream.read(MAX_TASK_BYTES + 1)
            after = os.fstat(stream.fileno())
        latest = path.lstat()
        if (not os.path.samestat(opened, latest) or latest.st_nlink != 1 or
                after.st_size != before.st_size or after.st_mtime_ns != before.st_mtime_ns or
                latest.st_size != after.st_size or latest.st_mtime_ns != after.st_mtime_ns):
            raise TransferError(409, "Source changed during exchange; refresh and retry.")
    except OSError as exc:
        raise TransferError(409, "Learner source could not be exchanged safely.") from exc
    if len(data) > MAX_TASK_BYTES:
        raise TransferError(413, "Source exchange exceeds the per-course size limit.")
    try:
        data.decode("utf-8")
    except UnicodeError as exc:
        raise TransferError(400, "Source exchange accepts UTF-8 text only.") from exc
    if b"\0" in data:
        raise TransferError(400, "Source exchange does not accept NUL bytes.")
    return data


def record(task_id: str, sources: dict[str, bytes]) -> dict:
    if task_id not in FOUNDATION_IDS:
        raise TransferError(400, "Only the five native foundation courses can be exchanged.")
    if sum(len(data) for data in sources.values()) > MAX_TASK_BYTES:
        raise TransferError(413, "Course sources exceed 512 KiB.")
    files = []
    for path, data in sources.items():
        try:
            content = data.decode("utf-8")
        except UnicodeError as exc:
            raise TransferError(400, "Source exchange accepts UTF-8 text only.") from exc
        if "\0" in content:
            raise TransferError(400, "Source exchange does not accept NUL bytes.")
        files.append({"path": path, "content": content, "sha256": sha(data)})
    result = {"task_id": task_id, "files": files}
    if encoded_size(result) > MAX_TASK_JSON:
        raise TransferError(413, "Encoded course sources exceed 768 KiB.")
    return result


def bundle(catalog: str, tasks: list[dict]) -> dict:
    value = {"schema_version": 1, "kind": KIND, "catalog_sha256": catalog,
             "origin": "native-windows", "tasks": tasks}
    if encoded_size(value) > MAX_BUNDLE_JSON:
        raise TransferError(413, "Source exchange file exceeds 4 MiB.")
    return value


def validate_import(value: dict, catalog: str, task_id: str, declared: list[str]) -> dict[str, bytes]:
    if (set(value) != {"schema_version", "kind", "catalog_sha256", "origin", "tasks"} or
            type(value["schema_version"]) is not int or value["schema_version"] != 1 or
            value["kind"] != KIND or value["origin"] != "native-windows"):
        raise TransferError(400, "Expected a source-only Windows exchange file, not an acceptance record.")
    if value["catalog_sha256"] != catalog:
        raise TransferError(409, "Course catalogs differ; use matching CppAppLab versions.")
    tasks = value["tasks"]
    if (task_id not in FOUNDATION_IDS or not isinstance(tasks, list) or len(tasks) != 1 or
            not isinstance(tasks[0], dict) or set(tasks[0]) != {"task_id", "files"} or
            tasks[0]["task_id"] != task_id):
        raise TransferError(400, "Import exactly the selected foundation course.")
    selected = tasks[0]
    if encoded_size(selected) > MAX_TASK_JSON:
        raise TransferError(413, "Encoded course sources exceed 768 KiB.")
    rows = selected["files"]
    if not isinstance(rows, list) or len(rows) != len(declared):
        raise TransferError(400, "Source file set does not match the course contract.")
    result = {}
    total = 0
    for row in rows:
        if (not isinstance(row, dict) or set(row) != {"path", "content", "sha256"} or
                not isinstance(row["path"], str) or row["path"] not in declared or
                row["path"] in result or not isinstance(row["content"], str)):
            raise TransferError(400, "Unexpected, duplicate or undeclared learner source.")
        try:
            data = row["content"].encode("utf-8")
        except UnicodeError as exc:
            raise TransferError(400, "Learner source must be valid UTF-8.") from exc
        total += len(data)
        if total > MAX_TASK_BYTES:
            raise TransferError(413, "Course sources exceed 512 KiB.")
        if b"\0" in data or row["sha256"] != sha(data):
            raise TransferError(400, "Learner source hash or text validation failed.")
        result[row["path"]] = data
    if set(result) != set(declared):
        raise TransferError(400, "Incomplete learner source set.")
    return result


def populate_fresh(targets: dict[str, Path], starters: dict[str, bytes],
                   imported: dict[str, bytes]) -> None:
    """Stage all bytes before publishing; only replace untouched fresh starters.

    The web server serializes foundation operations in its process. This is
    not a sandbox or a transaction against adversarial external editors.
    A disk failure may leave the new workspace incomplete, never an old one
    overwritten and never a fabricated PASS.
    """
    if set(targets) != set(starters) or set(targets) != set(imported):
        raise TransferError(400, "Incomplete fresh workspace contract.")
    staged = {}
    try:
        for name, path in targets.items():
            if source_bytes(path) != starters[name]:
                raise TransferError(409, "Linux source differs from the new starter; no automatic overwrite.")
            fd, temporary = tempfile.mkstemp(prefix=".source-reuse-", dir=path.parent)
            temp = Path(temporary)
            staged[name] = temp
            with os.fdopen(fd, "wb") as stream:
                stream.write(imported[name])
                stream.flush()
                os.fsync(stream.fileno())
            os.chmod(temp, stat.S_IMODE(path.lstat().st_mode))
        for name, path in targets.items():
            if source_bytes(path) != starters[name]:
                raise TransferError(409, "Linux source changed before import; no automatic overwrite.")
        for name, path in targets.items():
            if source_bytes(path) != starters[name]:
                raise TransferError(409, "Fresh workspace changed; inspect it before retrying.")
            os.replace(staged[name], path)
    except OSError as exc:
        raise TransferError(409, "Import was interrupted in the new workspace. Inspect its sources; no acceptance was recorded.") from exc
    finally:
        for temp in staged.values():
            temp.unlink(missing_ok=True)

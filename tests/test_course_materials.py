#!/usr/bin/env python3
"""Check learner-facing course completeness without installing dependencies."""
from __future__ import annotations

import csv
import re
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "course" / "tasks.tsv"
GUIDE = ROOT / "course" / "LEARNING_GUIDE_ZH.md"


def check_file(path: Path, label: str) -> str:
    if not path.is_file() or path.is_symlink():
        raise AssertionError(f"{label} is missing, not a regular file or is a symlink: {path}")
    text = path.read_text(encoding="utf-8")
    if not text.strip():
        raise AssertionError(f"{label} is empty: {path}")
    return text


def main() -> int:
    guide = check_file(GUIDE, "Chinese learning guide")
    if "中文学习导航" not in guide:
        raise AssertionError("The Chinese learning guide is missing its title")
    rows: list[list[str]] = []
    with CATALOG.open(newline="", encoding="utf-8") as stream:
        for raw_line in stream:
            if not raw_line.strip() or raw_line.lstrip().startswith("#"):
                continue
            row = next(csv.reader([raw_line], delimiter="\t"))
            if len(row) != 13:
                raise AssertionError(f"Expected 13 task fields, received {len(row)}: {row}")
            rows.append(row)

    if len(rows) != 23:
        raise AssertionError(f"Expected 23 released learner tasks; found {len(rows)}")

    counts: Counter[str] = Counter()
    previous_ids: set[str] = set()
    for row in rows:
        (
            task_id,
            title,
            week,
            order,
            lesson_path,
            hint_path,
            template_path,
            source_files,
            kind,
            expected_stdout,
            fixture_path,
            tests,
            prerequisites,
        ) = row
        if task_id in previous_ids:
            raise AssertionError(f"Duplicate task id: {task_id}")
        previous_ids.add(task_id)
        counts[f"W{week}"] += 1

        if task_id not in guide:
            raise AssertionError(f"Chinese guide has no entry for {task_id}")
        if not title or not order.isdigit():
            raise AssertionError(f"Task title/order missing: {task_id}")
        lesson = check_file(ROOT / lesson_path, f"lesson for {task_id}")
        hints = check_file(ROOT / hint_path, f"hints for {task_id}")
        template = ROOT / template_path
        if not template.is_dir() or template.is_symlink():
            raise AssertionError(f"Learner template missing for {task_id}: {template}")

        for filename in source_files.split(";"):
            path = template / filename
            if not path.is_file() or path.is_symlink():
                raise AssertionError(f"Declared learner source missing for {task_id}: {path}")

        if not re.search(r"(?m)^## Goal\b", lesson):
            raise AssertionError(f"Missing learning goal: {task_id}")
        if not re.search(r"(?m)^## Acceptance\b", lesson):
            raise AssertionError(f"Missing acceptance checklist: {task_id}")
        code_fence = chr(96) * 3
        if lesson.count(code_fence) % 2:
            raise AssertionError(f"Unbalanced Markdown code fences: {task_id}")
        if (chr(92) + code_fence) in lesson:
            raise AssertionError(f"Escaped Markdown code fence: {task_id}")
        for level in (1, 2, 3):
            if not re.search(r"(?m)^## Hint " + str(level) + r"\b", hints):
                raise AssertionError(f"Missing progressive hint {level}: {task_id}")

        if kind == "cmake_gtest":
            fixture = ROOT / fixture_path
            if not fixture.is_dir() or fixture.is_symlink():
                raise AssertionError(f"Fixed acceptance fixture missing for {task_id}: {fixture}")
            if not tests.isdigit() or int(tests) < 1:
                raise AssertionError(f"Fixed acceptance test count missing for {task_id}")
        elif kind != "exact_stdout":
            raise AssertionError(f"Unknown acceptance kind for {task_id}: {kind}")

    for week in range(1, 10):
        if not counts[f"W{week}"]:
            raise AssertionError(f"No released tasks in W{week}")
    if "W9-A4" not in guide or "W10" not in guide:
        raise AssertionError("The guide must distinguish unfinished future curriculum")

    print("[PASS] Course material audit: 23/23 registered tasks")
    print("[PASS] Every released task has a goal, acceptance, three hints and editable files")
    print("[PASS] Every released task is linked in the Chinese study route")
    print("[PASS] Released course coverage: " + ", ".join(f"{k}={counts[k]}" for k in sorted(counts)))
    print("[INFO] Future W9-A4/A5 and W10-W12 lessons remain explicitly unregistered")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

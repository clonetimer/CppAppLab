#!/usr/bin/env python3
"""Generate the P8 course coverage matrix from the authoritative task catalog.

Catalog presence is deliberately reported separately from deep journey coverage.
This script never infers PASS from a task being listed or selectable.
"""
from __future__ import annotations

import argparse
import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "course" / "tasks.tsv"
OUTPUT = ROOT / "artifacts" / "p8-final" / "acceptance" / "course-coverage-matrix.csv"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=OUTPUT)
    args = parser.parse_args()
    rows = []
    with CATALOG.open(encoding="utf-8", newline="") as source:
        lines = (line for line in source if line.strip() and not line.startswith("#"))
        for row in csv.reader(lines, delimiter="\t"):
            if len(row) != 13:
                raise SystemExit(f"Invalid task catalog row ({len(row)} columns): {row[:2]}")
            task_id, title, week, _order, lesson, _hints, _template, sources, acceptance, _stdout, _fixture, _tests, prereqs = row
            lesson_path = ROOT / lesson
            lesson_text = lesson_path.read_text(encoding="utf-8") if lesson_path.is_file() else ""
            start_here = "## Start here" in lesson_text
            rows.append({
                "task_id": task_id,
                "title": title,
                "week": week,
                "lesson_path": lesson,
                "task_type": "service" if task_id in {"w7.tcp-echo", "w7.tcp-chat", "w8.http-server"} else ("application" if task_id in {"w1.hello-world", "w1.calculator", "w2.todo-cli", "w3.task-manager", "w4.quality-gate", "w5.system-lab", "w6.thread-pool", "w8.http-protocol", "w8.http-parser", "w8.static-files", "w8.router"} else "library-test"),
                "source_files": sources,
                "acceptance_kind": acceptance,
                "prerequisites": prereqs,
                "lesson_exists": "YES" if lesson_text else "NO",
                "task_first_guidance": "YES" if start_here else "NO",
                "catalog_browser_coverage": "AUTOMATED_IN_CI" if task_id else "NOT_RUN",
                "deep_journey_coverage": "SEE_FEATURE_MATRIX",
                "formal_acceptance": "ENGINE_DEFINED",
                "result": "NOT_RATED_BY_CATALOG_GENERATOR",
                "evidence": "course/tasks.tsv; tests/test_web_workbench_browser.cjs",
            })
    if len(rows) != 26:
        raise SystemExit(f"Expected 26 catalog tasks, found {len(rows)}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    print(f"[PASS] Wrote {len(rows)} catalog rows to {args.output}")
    print("[INFO] Catalog coverage is not deep-journey or formal-acceptance PASS.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

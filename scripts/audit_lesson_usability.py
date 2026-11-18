#!/usr/bin/env python3
"""Audit structural usability signals across the authoritative 23-task lesson catalog.

This is a diagnostic inventory, not a comprehension score. Presence of a heading
or phrase is never treated as proof that a learner understands the material.
"""
from __future__ import annotations

import argparse
import csv
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "course" / "tasks.tsv"
DEFAULT_OUTPUT = ROOT / "artifacts" / "tutorial-usability" / "lesson-audit.csv"

CHECKS = {
    "task_first_path": lambda text: bool(re.search(r"^## Start here\s*$", text, re.M)),
    "goal": lambda text: bool(re.search(r"^## Goal\s*$", text, re.M)),
    "concrete_steps": lambda text: len(re.findall(r"^\s*\d+\.\s+", text, re.M)) >= 3,
    "mental_model": lambda text: bool(re.search(r"\*\*Mental model:", text, re.I)),
    "common_errors": lambda text: bool(re.search(r"common (error|mistake)|常见错误|troubleshoot|failure mode", text, re.I)),
    "success_criteria": lambda text: bool(re.search(r"acceptance|验收|expected result|expected output", text, re.I)),
    "hint_file": lambda text: False,
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    rows: list[dict[str, str]] = []
    with CATALOG.open(encoding="utf-8", newline="") as source:
        catalog = csv.reader(
            (line for line in source if line.strip() and not line.startswith("#")),
            delimiter="\t",
        )
        for item in catalog:
            if len(item) != 13:
                raise SystemExit(f"Invalid task catalog row ({len(item)} columns): {item[:2]}")
            task_id, title, week, _order, lesson, hints, _template, sources, acceptance, _stdout, _fixture, _tests, prereqs = item
            lesson_path = ROOT / lesson
            hint_path = ROOT / hints
            text = lesson_path.read_text(encoding="utf-8") if lesson_path.is_file() else ""
            values = {name: check(text) for name, check in CHECKS.items() if name != "hint_file"}
            values["hint_file"] = hint_path.is_file()
            rows.append({
                "task_id": task_id,
                "title": title,
                "week": week,
                "lesson_path": lesson,
                "hint_path": hints,
                "source_files": sources,
                "acceptance_kind": acceptance,
                "prerequisites": prereqs,
                **{name: "YES" if value else "NO" for name, value in values.items()},
                "review_required": "YES",
                "comprehension_result": "NOT_TESTED",
                "review_notes": "",
            })
    if len(rows) != 23:
        raise SystemExit(f"Expected 23 registered lessons, found {len(rows)}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    for field in CHECKS:
        count = sum(row[field] == "YES" for row in rows)
        print(f"[AUDIT] {field}: {count}/{len(rows)}")
    print(f"[PASS] Wrote structural audit for {len(rows)} lessons: {args.output}")
    print("[NOTE] Structural presence is not learner comprehension; manual/learner review remains required.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

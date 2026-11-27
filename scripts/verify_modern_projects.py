#!/usr/bin/env python3
"""Actual independent CP4 learner project red/green and fingerprint evidence."""
from __future__ import annotations
import argparse
import json
import os
import platform
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "web/server"))
import modern_projects as projects


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    report = {"status": "FAIL", "github_sha": os.environ.get("GITHUB_SHA"),
              "platform": platform.platform(), "projects": []}
    try:
        with tempfile.TemporaryDirectory(prefix="cppapplab-modern-project-replay-") as temporary:
            workspace = Path(temporary)
            for meta in projects.PROJECTS.values():
                key = meta["id"]
                item = {"id": key, "standard": meta["standard"], "expected_tests": meta["count"]}
                report["projects"].append(item)
                folder = ROOT / "course/modern_projects" / key
                initial = projects.start(ROOT, workspace, key)
                assert initial["source"] == (folder / "starter.cpp").read_text(encoding="utf-8")
                assert initial["state"] == "UNTESTED"
                failed = projects.check(ROOT, workspace, key, initial["revision"])
                item["starter"] = failed
                assert failed["status"] == "FAIL" and not failed["passed"], failed
                assert failed["steps"][-1]["phase"] == "test"
                assert failed["observed_tests"] == meta["count"]
                assert failed["classic_course_acceptance"] is False
                assert projects.snapshot(ROOT, workspace, key)["state"] == "FAIL"
                # CI supplies the reference only as a simulated learner action.
                reference = (folder / "reference.cpp").read_text(encoding="utf-8")
                saved = projects.save(ROOT, workspace, key, reference, initial["revision"])
                assert saved["state"] == "STALE"
                assert saved["source"] == reference
                passed = projects.check(ROOT, workspace, key, saved["revision"])
                item["reference_replay"] = passed
                assert passed["status"] == "PASS" and passed["passed"], passed
                assert passed["observed_tests"] == meta["count"]
                assert passed["classic_course_acceptance"] is False
                resumed = projects.start(ROOT, workspace, key)
                assert resumed["state"] == "PASS" and resumed["source"] == reference
                changed = projects.save(ROOT, workspace, key, reference + "\n// change after check\n",
                                        resumed["revision"])
                assert changed["state"] == "STALE"
                try:
                    projects.check(ROOT, workspace, key, resumed["revision"])
                except projects.ProjectError as exc:
                    assert exc.status == 409
                else:
                    raise AssertionError("stale revision was accepted")
                try:
                    projects.save(ROOT, workspace, key, "overwrite", resumed["revision"])
                except projects.ProjectError as exc:
                    assert exc.status == 409
                else:
                    raise AssertionError("stale save was accepted")
                assert projects.start(ROOT, workspace, key)["source"] == changed["source"]
                item["source_revision_guards"] = "PASS"
                item["fixture_digest"] = changed["contract_digest"]
                print(key, "starter FAIL; reference", meta["count"], "/", meta["count"],
                      "PASS; stale and resume PASS", flush=True)
            assert sorted(p.name for p in workspace.iterdir()) == [".modern"]
        report["status"] = "PASS"
    except Exception as exc:
        report["error"] = repr(exc)
        raise
    finally:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

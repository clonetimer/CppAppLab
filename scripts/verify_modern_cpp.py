#!/usr/bin/env python3
"""Actual CP4 GCC 13 feature gates and fixed negative/repair evidence."""
from __future__ import annotations
import argparse
import json
import os
import platform
import sys
import tempfile
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "web/server"))
import knowledge_lab
import modern_cpp


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    report = {"status": "FAIL", "github_sha": os.environ.get("GITHUB_SHA"),
              "platform": platform.platform(), "compiler": modern_cpp.COMPILER,
              "modern_replays": [], "baseline_checks": []}
    try:
        for topic in modern_cpp.TOPICS:
            for variant in ("failure", "repair"):
                result = knowledge_lab.run(topic["id"], variant)
                report["modern_replays"].append(result)
                print(topic["id"], variant, result["status"], result.get("compiler_version"), flush=True)
                assert result["observed"], result
                assert result["course_acceptance"] is False
                assert result["compiler_version"].startswith("13.")
                assert [step["phase"] for step in result["steps"][:2]] == ["toolchain", "feature-check"]
                assert all(step["exit_code"] == 0 for step in result["steps"][:2])
                last = result["steps"][-1]
                if variant == "repair":
                    assert last["phase"] == "run" and last["exit_code"] == 0 and last["output"].strip() == "42"
                else:
                    assert last["phase"] == topic["phase"] and last["exit_code"] > 0
                    assert topic["marker"].lower() in last["output"].lower()
        assert len(report["modern_replays"]) == 20
        with tempfile.TemporaryDirectory(prefix="cppapplab-dialect-") as directory:
            root = Path(directory)
            for topic_id in ("cpp20-concepts", "cpp23-expected"):
                topic = modern_cpp.BY_ID[topic_id]
                (root / "main.cpp").write_text(topic["good"], encoding="utf-8")
                step = knowledge_lab.execute([modern_cpp.COMPILER, "-std=c++17",
                                              "-pedantic-errors", "-fsyntax-only", "main.cpp"], root, 15)
                report["baseline_checks"].append({"topic": topic_id, "step": step})
                assert isinstance(step["exit_code"], int) and step["exit_code"] > 0
                assert not step["unavailable"] and not step["timed_out"]
            with patch.object(modern_cpp, "COMPILER", str(root / "missing-g++-13")):
                missing = knowledge_lab.run("cpp20-concepts", "failure")
                report["baseline_checks"].append({"missing_profile": missing})
                assert missing["status"] == "TOOLCHAIN_UNAVAILABLE" and not missing["observed"]
                classic = knowledge_lab.run("scope", "repair")
                report["baseline_checks"].append({"classic_still_runs": classic})
                assert classic["observed"]
                assert classic["steps"][0]["command"][0] == "g++"
                assert "-std=c++17" in classic["steps"][0]["command"]
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

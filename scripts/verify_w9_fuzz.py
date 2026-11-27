#!/usr/bin/env python3
"""Run the local A5 invariant target with real bounded Clang/libFuzzer/ASan/UBSan.

Trusted local learner source only; this is not an isolation sandbox.
No network, remote targets, arbitrary compiler arguments, or attack traffic.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import signal
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "course/fixtures/w9/http-fuzzing"
REFERENCE = ROOT / "labs/w9/http-fuzzing/reference"
A1 = ROOT / "labs/w9/http-chunked/reference"
A4 = ROOT / "labs/w9/http-hardening/reference"
RUNS = 10_000
MAX_LEN = 4096
MAX_SOURCE = 128 * 1024


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def execute(command: list[str], cwd: Path, logfile: Path, timeout: int,
            env: dict[str, str]) -> dict:
    record = {"command": command, "exit_code": None, "timed_out": False,
              "log": logfile.name}
    with logfile.open("wb") as log:
        try:
            proc = subprocess.Popen(command, cwd=cwd, env=env, stdin=subprocess.DEVNULL,
                                    stdout=log, stderr=subprocess.STDOUT,
                                    start_new_session=True)
        except OSError as exc:
            log.write(str(exc).encode())
            record["unavailable"] = True
            return record
        try:
            record["exit_code"] = proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            record["timed_out"] = True
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            record["exit_code"] = proc.wait(timeout=5)
    return record


def require_success(record: dict) -> None:
    if record["exit_code"] != 0 or record["timed_out"] or record.get("unavailable"):
        raise RuntimeError("Actual toolchain or campaign did not complete: " + record["log"])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--learner-root", type=Path, default=REFERENCE,
                        help="Explicit trusted local learner workspace, default reference (CI)")
    parser.add_argument("--output", type=Path, required=True,
                        help="New directory for evidence; never overwrite an existing run")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    report = {"status": "FAIL", "github_sha": os.environ.get("GITHUB_SHA"),
              "platform": platform.platform(), "requested_runs": RUNS,
              "max_len": MAX_LEN, "seed": 42, "steps": [], "source_digests": {},
              "seed_digests": {}, "findings": []}
    env = dict(os.environ, LC_ALL="C", LANG="C", ASAN_OPTIONS="detect_leaks=1:abort_on_error=1",
               UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    reproductions = output / "reproductions"
    reproductions.mkdir()
    try:
        compiler = shutil.which("clang++")
        if not compiler:
            raise RuntimeError("Optional clang++ toolchain is unavailable.")
        report["compiler"] = compiler
        version = execute([compiler, "--version"], ROOT, output / "compiler.log", 10, env)
        report["steps"].append(version)
        require_success(version)
        report["compiler_version"] = (output / "compiler.log").read_text(errors="replace").splitlines()[0]
        source = args.learner_root / "src/admission_invariants.cpp"
        if source.is_symlink() or not source.is_file() or source.stat().st_size > MAX_SOURCE:
            raise RuntimeError("Expected a regular bounded learner implementation file.")
        source_data = source.read_bytes()
        report["learner_source_sha256"] = sha(source_data)
        report["reference_replay"] = args.learner_root.resolve() == REFERENCE.resolve()
        with tempfile.TemporaryDirectory(prefix="cppapplab-w9-fuzz-") as temporary:
            stage = Path(temporary)
            staged = stage / "admission_invariants.cpp"
            staged.write_bytes(source_data)
            fixed = [
                A1 / "src/http_request_parser.cpp",
                A1 / "include/http_request_parser.hpp",
                A1 / "include/http_message.hpp",
                A4 / "src/http_hardening.cpp",
                ROOT / "course/fixtures/w9/http-hardening/include/http_hardening.hpp",
                FIXTURE / "include/admission_invariants.hpp",
                FIXTURE / "include/admission_replay.hpp",
                FIXTURE / "fuzz/admission_fuzz.cpp",
                FIXTURE / "fuzz/seeds.json",
            ]
            report["source_digests"] = {str(path.relative_to(ROOT)): sha(path.read_bytes()) for path in fixed}
            corpus = stage / "corpus"
            corpus.mkdir()
            seeds = json.loads((FIXTURE / "fuzz/seeds.json").read_text(encoding="utf-8"))
            if len(seeds) != 8:
                raise RuntimeError("Frozen seed manifest must contain eight seeds.")
            for seed in seeds:
                name = seed["id"]
                if not re.fullmatch(r"[a-z0-9-]+", name) or name in report["seed_digests"]:
                    raise RuntimeError("Invalid or duplicate frozen seed name.")
                data = seed["data"].encode("utf-8")
                if not 0 < len(data) <= MAX_LEN:
                    raise RuntimeError("Seed outside campaign limits.")
                (corpus / name).write_bytes(data)
                report["seed_digests"][name] = sha(data)
            binary = stage / "admission-fuzz"
            command = [
                compiler, "-std=c++17", "-O1", "-g",
                "-fsanitize=fuzzer,address,undefined", "-fno-sanitize-recover=all",
                "-fno-omit-frame-pointer",
                "-I", str(FIXTURE / "include"),
                "-I", str(ROOT / "course/fixtures/w9/http-hardening/include"),
                "-I", str(A1 / "include"),
                str(A1 / "src/http_request_parser.cpp"),
                str(A4 / "src/http_hardening.cpp"), str(staged),
                str(FIXTURE / "fuzz/admission_fuzz.cpp"), "-o", str(binary),
            ]
            compile_step = execute(command, stage, output / "compile.log", 120, env)
            report["steps"].append(compile_step)
            require_success(compile_step)
            command = [
                str(binary), str(corpus), f"-runs={RUNS}", f"-max_len={MAX_LEN}",
                "-seed=42", "-timeout=3", "-max_total_time=60", "-rss_limit_mb=1024",
                "-artifact_prefix=" + str(reproductions) + "/",
            ]
            campaign = execute(command, stage, output / "fuzz.log", 90, env)
            report["steps"].append(campaign)
            report["findings"] = [
                {"file": str(path.relative_to(output)), "sha256": sha(path.read_bytes()),
                 "bytes": path.stat().st_size}
                for path in sorted(reproductions.iterdir()) if path.is_file()
            ]
            require_success(campaign)
            log = (output / "fuzz.log").read_text(encoding="utf-8", errors="replace")
            done = re.findall(r"(?m)^#(\d+)\s+DONE\s+cov:\s*(\d+)\s+ft:\s*(\d+)", log)
            if not done:
                raise RuntimeError("No actual final libFuzzer coverage counters.")
            observed, coverage, features = map(int, done[-1])
            report.update(observed_runs=observed, coverage=coverage, features=features,
                          final_corpus_files=sum(1 for path in corpus.iterdir() if path.is_file()))
            if observed != RUNS or coverage <= 0 or features <= 0 or report["findings"]:
                raise RuntimeError("Requested run count, coverage or reproduction gate not met.")
            if not re.search(rf"Done {RUNS} runs in ", log):
                raise RuntimeError("Final libFuzzer execution summary missing.")
            report["status"] = "PASS"
            print(f"PASS: actual libFuzzer {observed}/{RUNS} runs, cov={coverage}, ft={features}, findings=0",
                  flush=True)
    except Exception as exc:
        report["error"] = repr(exc)
        raise
    finally:
        (output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                                            encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

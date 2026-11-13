#!/usr/bin/env python3
"""Exercise CppAppLab's first-use confirmation with and without a controlling TTY."""

from __future__ import annotations

import os
import pty
import select
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "scripts" / "lib" / "terminal.sh"


def run_with_controlling_tty(response: str) -> tuple[int, str]:
    pid, master_fd = pty.fork()
    if pid == 0:
        command = (
            'source "$1"; '
            'exec </dev/null; '
            'cppapplab_prompt_for_confirmation "Approve package installation? [Y/n] "'
        )
        os.execl("/bin/bash", "bash", "-c", command, "cppapplab-tty-test", str(HELPER))

    output = bytearray()
    deadline = time.monotonic() + 5.0
    try:
        while b"Approve package installation?" not in output:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise AssertionError("Confirmation prompt was not written to the controlling terminal.")
            readable, _, _ = select.select([master_fd], [], [], remaining)
            if not readable:
                continue
            chunk = os.read(master_fd, 4096)
            if not chunk:
                raise AssertionError("Child exited before displaying the confirmation prompt.")
            output.extend(chunk)

        os.write(master_fd, response.encode("utf-8"))
        while True:
            readable, _, _ = select.select([master_fd], [], [], 5.0)
            if not readable:
                raise AssertionError("Confirmation helper did not exit after receiving a response.")
            try:
                chunk = os.read(master_fd, 4096)
            except OSError:
                break
            if not chunk:
                break
            output.extend(chunk)
    finally:
        os.close(master_fd)

    _, status = os.waitpid(pid, 0)
    return os.waitstatus_to_exitcode(status), output.decode("utf-8", errors="replace")


def main() -> int:
    yes_status, yes_output = run_with_controlling_tty("y\\n")
    if yes_status != 0:
        raise AssertionError(f"TTY confirmation should accept yes (status={yes_status}): {yes_output}")

    no_status, no_output = run_with_controlling_tty("n\\n")
    if no_status != 1:
        raise AssertionError(f"TTY confirmation should return 1 for no (status={no_status}): {no_output}")

    command = (
        'source "$1"; '
        'cppapplab_prompt_for_confirmation "Approve package installation? [Y/n] "'
    )
    no_tty = subprocess.run(
        ["/bin/bash", "-c", command, "cppapplab-no-tty-test", str(HELPER)],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
        start_new_session=True,
        timeout=5,
    )
    if no_tty.returncode != 2:
        raise AssertionError(
            "Confirmation without stdin or a controlling TTY must fail safely "
            f"(status={no_tty.returncode}, stdout={no_tty.stdout!r}, stderr={no_tty.stderr!r})."
        )
    if "No interactive terminal" not in no_tty.stderr:
        raise AssertionError(f"Missing actionable no-terminal diagnostic: {no_tty.stderr!r}")

    print("[PASS] Confirmation reads from /dev/tty when stdin is redirected.")
    print("[PASS] Explicit yes/no responses are handled correctly.")
    print("[PASS] Non-interactive execution fails safely without installing packages.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

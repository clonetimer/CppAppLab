#!/usr/bin/env python3
"""Platform-owned byte-exact CLI integration checks for the W8 Router."""

from __future__ import annotations

import pathlib
import subprocess
import sys
import tempfile


def run_cli(executable: str, document_root: pathlib.Path, request: bytes) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        [executable, "parse-stdin", str(document_root)],
        input=request,
        capture_output=True,
        timeout=5.0,
        check=False,
    )


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: router_smoke.py <http-router>", file=sys.stderr)
        return 2

    executable = sys.argv[1]
    help_result = subprocess.run(
        [executable, "--help"], capture_output=True, text=True, timeout=5.0, check=False
    )
    require(help_result.returncode == 0, f"unexpected --help exit code: {help_result.returncode}")
    require("parse-stdin" in help_result.stdout, "Router help does not advertise parse-stdin")

    with tempfile.TemporaryDirectory(prefix="CppAppLab router smoke ") as temporary:
        web_root = pathlib.Path(temporary) / "www"
        web_root.mkdir()
        (web_root / "index.html").write_bytes(b"home")
        (web_root / "hello").write_bytes(b"shadow")
        (web_root / "echo").write_bytes(b"shadow")

        hello_body = b"Hello from C++ HTTP Server!\n"
        hello_request = (
            b"GET /hello?x=1 HTTP/1.1\r\n"
            b"Host: localhost\r\n\r\n"
        )
        hello_result = run_cli(executable, web_root, hello_request)
        expected_hello = (
            b"HTTP/1.1 200 OK\r\n"
            b"Content-Type: text/plain; charset=utf-8\r\n"
            + f"Content-Length: {len(hello_body)}\r\n".encode("ascii")
            + b"Connection: close\r\n\r\n"
            + hello_body
        )
        require(hello_result.returncode == 0, f"hello route exit code: {hello_result.returncode}")
        require(hello_result.stdout == expected_hello, f"hello response mismatch: {hello_result.stdout!r}")

        echo_body = b"A\x00B\nC"
        echo_request = (
            b"POST /echo HTTP/1.1\r\n"
            b"Host: localhost\r\n"
            b"Content-Length: 5\r\n\r\n"
            + echo_body
        )
        echo_result = run_cli(executable, web_root, echo_request)
        expected_echo = (
            b"HTTP/1.1 200 OK\r\n"
            b"Content-Type: text/plain; charset=utf-8\r\n"
            b"Content-Length: 5\r\n"
            b"Connection: close\r\n\r\n"
            + echo_body
        )
        require(echo_result.returncode == 0, f"binary echo exit code: {echo_result.returncode}")
        require(echo_result.stdout == expected_echo, f"binary echo response mismatch: {echo_result.stdout!r}")

        health_result = run_cli(
            executable,
            web_root,
            b"GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n",
        )
        require(health_result.returncode == 0, f"health exit code: {health_result.returncode}")
        require(b'{"status":"ok"}' in health_result.stdout, "health response body missing")
        require(b"Content-Type: application/json" in health_result.stdout, "health content type missing")

        root_result = run_cli(
            executable,
            web_root,
            b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n",
        )
        require(root_result.returncode == 0, f"root route exit code: {root_result.returncode}")
        require(b"home" in root_result.stdout, "static index fallback missing")

        get_echo_result = run_cli(
            executable,
            web_root,
            b"GET /echo HTTP/1.1\r\nHost: localhost\r\n\r\n",
        )
        require(get_echo_result.returncode == 0, f"GET /echo exit code: {get_echo_result.returncode}")
        require(b"405 Method Not Allowed" in get_echo_result.stdout, "GET /echo did not return 405")
        require(b"Allow: POST" in get_echo_result.stdout, "GET /echo did not advertise Allow: POST")
        require(b"shadow" not in get_echo_result.stdout, "GET /echo exposed the shadow file")

        bad_header_result = run_cli(
            executable,
            web_root,
            b"GET /hello HTTP/1.1\r\nHost : localhost\r\n\r\n",
        )
        require(bad_header_result.returncode == 1, f"bad-header exit code: {bad_header_result.returncode}")
        require(b"400 Bad Request" in bad_header_result.stdout, "malformed Host header did not return 400")

    print("Router CLI integration passed: hello, binary echo, health, static fallback, 405, parser error")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"Router smoke failed: {error}", file=sys.stderr)
        raise

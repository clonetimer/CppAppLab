#!/usr/bin/env python3
"""Platform-owned loopback smoke test for the W8 HTTP server CLI."""

from __future__ import annotations

import socket
import subprocess
import sys
import time


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: http_server_smoke.py <http-server>", file=sys.stderr)
        return 2

    binary = sys.argv[1]
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]

    process = subprocess.Popen(
        [binary, "server", str(port)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    try:
        deadline = time.monotonic() + 5.0
        response = None
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError("HTTP server exited before accepting requests")
            try:
                with socket.create_connection(("127.0.0.1", port), timeout=0.25) as client:
                    client.sendall(
                        b"GET /health HTTP/1.1\r\n"
                        b"Host: localhost\r\n"
                        b"Connection: close\r\n\r\n"
                    )
                    chunks = []
                    while True:
                        data = client.recv(4096)
                        if not data:
                            break
                        chunks.append(data)
                    response = b"".join(chunks)
                    if b"HTTP/1.1 200 OK\r\n" not in response:
                        raise RuntimeError(f"unexpected HTTP status: {response!r}")
                    if response.split(b"\r\n\r\n", 1)[1] != b'{"status":"ok"}':
                        raise RuntimeError(f"unexpected HTTP body: {response!r}")
                    break
            except OSError:
                time.sleep(0.05)
        if response is None:
            raise RuntimeError("HTTP server did not start listening")
    finally:
        if process.poll() is None:
            process.terminate()
        try:
            process.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=2.0)
        process.communicate()

    print("W8 HTTP server loopback CLI smoke passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"HTTP server smoke failed: {error}", file=sys.stderr)
        raise

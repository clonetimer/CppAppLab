#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 1 ]]; then
    echo "usage: http_server_smoke.sh <http-server>" >&2
    exit 2
fi
python3 - "$1" <<'PY'
import socket
import subprocess
import sys
import time

binary = sys.argv[1]
with socket.socket() as reservation:
    reservation.bind(("127.0.0.1", 0))
    port = reservation.getsockname()[1]

process = subprocess.Popen([binary, "server", str(port)], stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, text=True)
try:
    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError("HTTP server exited before accepting requests")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.25) as client:
                client.sendall(b"GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n")
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
    else:
        raise RuntimeError("HTTP server did not start listening")
finally:
    process.terminate()
    try:
        process.wait(timeout=2.0)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=2.0)
print("W8 HTTP server loopback CLI smoke passed")
PY

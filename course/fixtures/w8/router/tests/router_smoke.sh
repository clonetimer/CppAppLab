#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 1 ]] || exit 2
exe="$(realpath "$1")"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
mkdir "$work/www"
printf 'home' > "$work/www/index.html"
printf 'shadow' > "$work/www/hello"
printf 'shadow' > "$work/www/echo"
"$exe" --help | grep -Fq 'parse-stdin'
printf 'GET /hello?x=1 HTTP/1.1\r\nHost: localhost\r\n\r\n' |
    "$exe" parse-stdin "$work/www" > "$work/actual"
# Derive the Hello byte length independently rather than rely on a hand count.
hello_size="$(printf 'Hello from C++ HTTP Server!\n' | wc -c | tr -d ' ')"
printf 'HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: %s\r\nConnection: close\r\n\r\nHello from C++ HTTP Server!\n' "$hello_size" > "$work/expected"
cmp "$work/expected" "$work/actual"
printf 'POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nA\0B\nC' |
    "$exe" parse-stdin "$work/www" > "$work/actual"
printf 'HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: 5\r\nConnection: close\r\n\r\nA\0B\nC' > "$work/expected"
cmp "$work/expected" "$work/actual"
printf 'GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n' |
    "$exe" parse-stdin "$work/www" > "$work/actual"
grep -Fq '{"status":"ok"}' "$work/actual"
grep -Fq 'Content-Type: application/json' "$work/actual"
printf 'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n' |
    "$exe" parse-stdin "$work/www" > "$work/actual"
grep -Fq 'home' "$work/actual"
printf 'GET /echo HTTP/1.1\r\nHost: localhost\r\n\r\n' |
    "$exe" parse-stdin "$work/www" > "$work/actual"
grep -Fq '405 Method Not Allowed' "$work/actual"
grep -Fq 'Allow: POST' "$work/actual"
! grep -Fq 'shadow' "$work/actual"
set +e
printf 'GET /hello HTTP/1.1\r\nHost : localhost\r\n\r\n' |
    "$exe" parse-stdin "$work/www" > "$work/bad" 2> "$work/error"
status=$?
set -e
[[ "$status" -eq 1 ]]
grep -Fq '400 Bad Request' "$work/bad"
echo 'Router CLI integration passed: hello, binary echo, health, static fallback, 405, parser error'

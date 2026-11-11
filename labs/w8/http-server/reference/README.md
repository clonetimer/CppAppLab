# W8-D5 HTTP TCP Server

This target is Linux-only and builds on the repository's W6 ThreadPool and W8-D1–D4 HTTP implementation.

From the repository root, build and run the reference server:

```bash
cmake -S labs/w8/http-server/reference -B build-http-server -DCMAKE_BUILD_TYPE=Debug
cmake --build build-http-server --parallel
ctest --test-dir build-http-server --output-on-failure
./build-http-server/http-server server 8080 labs/w8/router/reference/www
```

The final argument is the static document root. If omitted, the server uses `www` relative to the process working directory; create that directory and an `index.html` before requesting `/`.

Try `curl -i http://127.0.0.1:8080/hello`, `curl -i http://127.0.0.1:8080/health`, `curl -i http://127.0.0.1:8080/` and `curl -i -X POST --data-binary 'hello' http://127.0.0.1:8080/echo`.

One request is served per TCP connection because the D1 response serializer emits `Connection: close`. Keep-Alive belongs to D6. The caller must join the thread running `HttpServer::run()` before destroying the server object.

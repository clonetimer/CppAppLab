# W8-D5 HTTP TCP Server

This target is Linux-only and builds on the repository's W6 ThreadPool and W8-D1–D4 HTTP implementation.

From the learner workspace:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
mkdir -p www
printf '<h1>CppAppLab</h1>\n' > www/index.html
./build/http-server server 8080 www
```

Try the dynamic `/hello`, `/health` and `/echo` endpoints, then `/` for the local `www/index.html`.

One request is served per TCP connection because the D1 response serializer emits `Connection: close`. Keep-Alive belongs to D6. The caller must join the thread running `HttpServer::run()` before destroying the server object.

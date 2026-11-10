# W8-D5 HTTP TCP Server

This target is Linux-only and builds on the repository's W6 ThreadPool and W8-D1–D4 HTTP implementation.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/http-server server 8080
```

One request is served per TCP connection because the D1 response serializer emits `Connection: close`. Keep-Alive belongs to D6.

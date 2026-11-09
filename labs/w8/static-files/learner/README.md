# W8-D3 · HTTP Responses and Static Files

Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Run the one-request handler demonstration from a directory containing `www/`:

```bash
./build/http-static-files handle GET /
./build/http-static-files handle GET /hello.txt
./build/http-static-files handle POST /
```

This task builds on W8-D1's validated response serializer and W8-D2's request model. It does not yet provide a listening TCP server.

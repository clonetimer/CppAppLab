# W6 · Thread Pool and Futures

The W6 exercise builds a fixed-size C++17 thread pool on the W5 bounded queue.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/thread-pool
```

## Platform acceptance

```bash
./build/cppapplab check "/absolute/path/to/your/thread-pool-workspace"
```

The fixed platform fixture uses its own headers and CMake project. It runs 22 platform tests, ten learner-authored tests and one CLI smoke test.

Local progress is navigation data, not a certificate.

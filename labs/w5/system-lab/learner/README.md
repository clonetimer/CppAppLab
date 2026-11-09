# W5 · Linux System Programming Lab

Implement the bounded queue and POSIX process runner described in the lesson.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

## CLI examples

```bash
./build/system-lab /bin/echo "hello from child"
./build/system-lab /bin/sh -c 'exit 6'
```

## Platform acceptance

```bash
./build/cppapplab check "/absolute/path/to/your/system-lab-workspace"
```

The platform supplies its own fixed headers and CMake fixture. Your CMakeLists.txt is for local learning and is not the acceptance authority.

Local progress is navigation data, not a certificate.

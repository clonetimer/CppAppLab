# W2 · Persistent Todo CLI

Implement `src/todo.cpp` using the fixed public interface in `include/todo.hpp`. The command-line front end is provided in `src/main.cpp`.

Build locally:

```bash
mkdir -p build
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -I include \
  src/todo.cpp src/main.cpp -o build/todo
```

Use a separate data file while experimenting:

```bash
export CPPAPPLAB_TODO_FILE="$PWD/todos.tsv"
./build/todo add "Review STL containers"
./build/todo list
./build/todo done 1
./build/todo remove 1
```

The platform check compiles your implementation against its own copy of the interface and runs fixed tests. Your CMake files and tests are not used as the acceptance authority.

The local progress record is navigation data, not a certificate.

# W3 · Task / TaskManager Refactor

Refactor the W2 Todo core into `Task` and `TaskManager`. Implement both source files while keeping the existing CLI and W2 data format.

Build:

```bash
mkdir -p build
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -I include \
  src/task.cpp src/task_manager.cpp src/main.cpp -o build/todo
```

Use a separate data file while testing:

```bash
export CPPAPPLAB_TODO_FILE="$PWD/todos.tsv"
./build/todo add "Review unique ownership"
./build/todo list
./build/todo done 1
```

The platform acceptance build uses its own copies of the public headers and fixed tests. Learner build files and tests are not the acceptance authority.

Local progress is navigation data, not a certificate.

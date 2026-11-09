#include "cppapplab/task.hpp"

#include <cstdlib>

#ifndef CPPAPPLAB_DATA_DIR
#define CPPAPPLAB_DATA_DIR "."
#endif

namespace cppapplab {

const std::vector<Task>& task_catalog() {
    static const std::vector<Task> tasks{
        {
            "w1.hello-world",
            "W1 · First C++ Program",
            1,
            1,
            "course/lessons/w1/hello-world.md",
            "course/lessons/w1/hello-world.hints.md",
            "labs/w1/hello-world/learner",
            {"src/main.cpp"},
            AcceptanceKind::ExactStdout,
            "Hello, CppAppLab!\n"
        }
    };

    return tasks;
}

const Task* find_task(std::string_view id) {
    for (const Task& task : task_catalog()) {
        if (task.id == id) {
            return &task;
        }
    }
    return nullptr;
}

std::filesystem::path data_root() {
    if (const char* override_path = std::getenv("CPPAPPLAB_DATA_DIR");
        override_path != nullptr && override_path[0] != '\0') {
        return std::filesystem::absolute(override_path).lexically_normal();
    }

    return std::filesystem::path(CPPAPPLAB_DATA_DIR).lexically_normal();
}

}  // namespace cppapplab

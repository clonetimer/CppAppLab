#include "task_manager.hpp"

#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {

std::filesystem::path storage_path() {
    if (const char* override_path = std::getenv("CPPAPPLAB_TODO_FILE");
        override_path != nullptr && override_path[0] != '\0') {
        return std::filesystem::absolute(override_path).lexically_normal();
    }

    if (const char* home = std::getenv("HOME"); home != nullptr && home[0] != '\0') {
        return std::filesystem::path(home) / ".local" / "share" / "cppapplab" / "todos.tsv";
    }

    return std::filesystem::current_path() / ".cppapplab-todo.tsv";
}

bool parse_id(std::string_view text, std::uint64_t& id) {
    if (text.empty()) {
        return false;
    }
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto parsed = std::from_chars(begin, end, id);
    return parsed.ec == std::errc{} && parsed.ptr == end && id != 0U;
}

void print_usage() {
    std::cerr << "Usage:\n"
              << "  todo add <title>\n"
              << "  todo list\n"
              << "  todo done <id>\n"
              << "  todo remove <id>\n";
}

int print_error(const cppapplab::tasks::Result& result) {
    std::cerr << "error: " << result.message << '\n';
    return result.error == cppapplab::tasks::Error::IoError ||
                   result.error == cppapplab::tasks::Error::InvalidFormat
               ? 4
               : 3;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 2;
    }

    cppapplab::tasks::TaskManager manager;
    const auto path = storage_path();
    const auto loaded = manager.load(path);
    if (!loaded) {
        return print_error(loaded);
    }

    const std::string_view command{argv[1]};
    if (command == "add") {
        if (argc != 3) {
            print_usage();
            return 2;
        }

        const std::uint64_t id = manager.next_id();
        if (id == 0U) {
            std::cerr << "error: no task IDs remain\n";
            return 3;
        }

        const auto result = manager.add(id, argv[2]);
        if (!result) {
            return print_error(result);
        }
        const auto saved = manager.save(path);
        if (!saved) {
            return print_error(saved);
        }
        std::cout << "Added #" << id << ": " << argv[2] << '\n';
        return 0;
    }

    if (command == "list") {
        if (argc != 2) {
            print_usage();
            return 2;
        }
        if (manager.empty()) {
            std::cout << "No tasks.\n";
            return 0;
        }
        for (const auto& task : manager.snapshot()) {
            std::cout << task << '\n';
        }
        return 0;
    }

    if (command == "done" || command == "remove") {
        if (argc != 3) {
            print_usage();
            return 2;
        }

        std::uint64_t id = 0U;
        if (!parse_id(argv[2], id)) {
            std::cerr << "error: task ID must be a positive integer\n";
            return 2;
        }

        const auto result = command == "done"
                                ? manager.mark_done(id)
                                : manager.remove(id);
        if (!result) {
            return print_error(result);
        }

        const auto saved = manager.save(path);
        if (!saved) {
            return print_error(saved);
        }
        std::cout << (command == "done" ? "Completed #" : "Removed #") << id << '\n';
        return 0;
    }

    std::cerr << "error: unknown command\n";
    print_usage();
    return 2;
}

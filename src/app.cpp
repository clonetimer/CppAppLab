#include "cppapplab/app.hpp"

#ifndef CPPAPPLAB_VERSION
#define CPPAPPLAB_VERSION "0.1.0-dev"
#endif

namespace cppapplab {

std::string_view version() noexcept {
    return CPPAPPLAB_VERSION;
}

std::string_view usage() noexcept {
    return "CppAppLab - C++ learning and engineering practice\n"
           "\n"
           "Usage:\n"
           "  cppapplab                                        Open interactive learning home\n"
           "  cppapplab menu                                   Open interactive learning home\n"
           "  cppapplab --help                                  Show this help\n"
           "  cppapplab --version                               Show version\n"
           "  cppapplab guide                                   中文学习路线与常见问题\n"
           "  cppapplab list                                    List available tasks\n"
           "  cppapplab progress                                Show course progress\n"
           "  cppapplab learn [task-id]                         Guided start or resume\n"
           "  cppapplab resume <task-id>                        Resume the saved workspace\n"
           "  cppapplab history <task-id>                       Show previous check attempts\n"
           "  cppapplab start <task-id> <workspace-directory>   Create learner workspace\n"
           "  cppapplab lesson <workspace-directory>            Read current task card\n"
           "  cppapplab hint <workspace-directory> <1|2|3>       Show a progressive hint\n"
           "  cppapplab check <workspace-directory>              Build and check current source\n"
           "  cppapplab status <workspace-directory>             Show current evidence status\n"
           "  cppapplab next <workspace-directory>               Show the next learning step\n";
}

}  // namespace cppapplab

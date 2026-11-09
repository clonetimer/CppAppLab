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
           "  cppapplab --help       Show this help\n"
           "  cppapplab --version    Show version\n";
}

}  // namespace cppapplab

#include "admission_replay.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string_view input(reinterpret_cast<const char*>(data), size);
    if (cppapplab::http::verify_admission_input(input) !=
        cppapplab::http::InvariantViolation::None) {
        std::abort();
    }
    return 0;
}

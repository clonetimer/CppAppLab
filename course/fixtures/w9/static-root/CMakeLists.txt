cmake_minimum_required(VERSION 3.18)

project(CppAppLabW9SecureStaticRootAcceptance LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(GTest CONFIG REQUIRED)

if(NOT DEFINED CPPAPPLAB_LEARNER_SOURCE_DIR)
    message(FATAL_ERROR "CPPAPPLAB_LEARNER_SOURCE_DIR is required.")
endif()

set(LEARNER "${CPPAPPLAB_LEARNER_SOURCE_DIR}")
foreach(required IN ITEMS
    "${LEARNER}/include/http_message.hpp"
    "${LEARNER}/include/http_request_target.hpp"
    "${LEARNER}/include/static_file_handler.hpp"
    "${LEARNER}/src/http_request_target.cpp"
    "${LEARNER}/src/static_file_handler.cpp")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Required learner static-root source is missing: ${required}")
    endif()
endforeach()

add_library(secure_static_root_under_test STATIC
    "${LEARNER}/src/http_request_target.cpp"
    "${LEARNER}/src/static_file_handler.cpp"
)
target_include_directories(secure_static_root_under_test PUBLIC "${LEARNER}/include")
target_compile_options(secure_static_root_under_test PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Werror
)

add_executable(secure_static_root_acceptance_tests
    tests/secure_static_root_acceptance_test.cpp
)
target_link_libraries(secure_static_root_acceptance_tests PRIVATE
    secure_static_root_under_test
    GTest::gtest_main
)
target_compile_options(secure_static_root_acceptance_tests PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Werror
)

include(GoogleTest)
gtest_discover_tests(secure_static_root_acceptance_tests PROPERTIES TIMEOUT 10)

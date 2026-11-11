cmake_minimum_required(VERSION 3.18)

project(CppAppLabW9HttpChunkedAcceptance LANGUAGES CXX)

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
    "${LEARNER}/include/http_request_parser.hpp"
    "${LEARNER}/src/http_request_parser.cpp")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Required learner HTTP parser source is missing: ${required}")
    endif()
endforeach()

add_library(http_chunked_under_test STATIC
    "${LEARNER}/src/http_request_parser.cpp"
)
target_include_directories(http_chunked_under_test PUBLIC "${LEARNER}/include")
target_compile_options(http_chunked_under_test PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Werror
)

add_executable(http_chunked_acceptance_tests
    tests/http_chunked_acceptance_test.cpp
)
target_link_libraries(http_chunked_acceptance_tests PRIVATE
    http_chunked_under_test
    GTest::gtest_main
)
target_compile_options(http_chunked_acceptance_tests PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Werror
)

include(GoogleTest)
gtest_discover_tests(http_chunked_acceptance_tests PROPERTIES TIMEOUT 10)

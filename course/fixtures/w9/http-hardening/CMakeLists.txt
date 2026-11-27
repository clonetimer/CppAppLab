cmake_minimum_required(VERSION 3.18)
project(CppAppLabW9HttpAdmissionAcceptance LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)
find_package(GTest CONFIG REQUIRED)

if(NOT DEFINED CPPAPPLAB_LEARNER_SOURCE_DIR)
    message(FATAL_ERROR "CPPAPPLAB_LEARNER_SOURCE_DIR is required.")
endif()
set(A1_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../labs/w9/http-chunked/reference")
set(LEARNER_SOURCE "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src/http_hardening.cpp")
add_library(http_admission STATIC
    "${A1_ROOT}/src/http_request_parser.cpp"
    "${LEARNER_SOURCE}")
target_include_directories(http_admission PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${A1_ROOT}/include")
target_compile_options(http_admission PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)

add_executable(http_admission_tests tests/http_admission_test.cpp)
target_link_libraries(http_admission_tests PRIVATE http_admission GTest::gtest_main)
target_compile_options(http_admission_tests PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
include(GoogleTest)
gtest_discover_tests(http_admission_tests PROPERTIES TIMEOUT 10)

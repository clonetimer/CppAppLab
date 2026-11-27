cmake_minimum_required(VERSION 3.18)
project(CppAppLabW9AdmissionInvariantAcceptance LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)
find_package(GTest CONFIG REQUIRED)
if(NOT DEFINED CPPAPPLAB_LEARNER_SOURCE_DIR)
    message(FATAL_ERROR "CPPAPPLAB_LEARNER_SOURCE_DIR is required.")
endif()
set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
set(A1 "${ROOT}/labs/w9/http-chunked/reference")
set(A4 "${ROOT}/labs/w9/http-hardening/reference")
add_library(admission_oracle STATIC
    "${A1}/src/http_request_parser.cpp"
    "${A4}/src/http_hardening.cpp"
    "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src/admission_invariants.cpp")
target_include_directories(admission_oracle PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${ROOT}/course/fixtures/w9/http-hardening/include"
    "${A1}/include")
target_compile_options(admission_oracle PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
add_executable(admission_oracle_tests tests/admission_invariants_test.cpp)
target_link_libraries(admission_oracle_tests PRIVATE admission_oracle GTest::gtest_main)
target_compile_options(admission_oracle_tests PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
include(GoogleTest)
gtest_discover_tests(admission_oracle_tests PROPERTIES TIMEOUT 10)

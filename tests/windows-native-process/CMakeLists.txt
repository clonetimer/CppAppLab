cmake_minimum_required(VERSION 3.18)

project(CppAppLabWindowsNativeProcessAcceptance LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This isolated acceptance project must run on native Windows.")
endif()
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "Windows native process acceptance requires real GCC/G++, not MSVC or Clang.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(FetchContent)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.18.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(googletest)

find_package(Threads REQUIRED)

get_filename_component(CPPAPPLAB_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

add_executable(
    cppapplab_process_test_child
    "${CPPAPPLAB_ROOT}/tests/process_test_child.cpp"
)
target_compile_definitions(cppapplab_process_test_child PRIVATE _WIN32_WINNT=0x0A00)
target_compile_options(
    cppapplab_process_test_child
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Werror
)
target_link_libraries(cppapplab_process_test_child PRIVATE Threads::Threads)

add_executable(
    cppapplab_process_tests
    "${CPPAPPLAB_ROOT}/src/process_win32.cpp"
    "${CPPAPPLAB_ROOT}/tests/process_test.cpp"
)
target_include_directories(
    cppapplab_process_tests
    PRIVATE
    "${CPPAPPLAB_ROOT}/include"
)
target_compile_definitions(cppapplab_process_tests PRIVATE _WIN32_WINNT=0x0A00)
target_compile_options(
    cppapplab_process_tests
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Werror
)
target_link_libraries(
    cppapplab_process_tests
    PRIVATE
    GTest::gtest_main
    Threads::Threads
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/cppapplab_process_test_config.hpp"
    CONTENT "#pragma once\n#define CPPAPPLAB_PROCESS_TEST_CHILD R\"CPPAPPLAB_PATH($<TARGET_FILE:cppapplab_process_test_child>)CPPAPPLAB_PATH\"\n"
)
target_include_directories(
    cppapplab_process_tests
    PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
)
add_dependencies(cppapplab_process_tests cppapplab_process_test_child)

include(GoogleTest)
gtest_discover_tests(
    cppapplab_process_tests
    PROPERTIES TIMEOUT 10
)

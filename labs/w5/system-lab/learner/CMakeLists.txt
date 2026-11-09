cmake_minimum_required(VERSION 3.18)

project(CppAppLabW5SystemProgramming LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)
find_package(Threads REQUIRED)

add_library(system_lab_options INTERFACE)
target_compile_features(system_lab_options INTERFACE cxx_std_17)
target_compile_options(system_lab_options INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Werror)

add_library(system_lab_core src/process_runner.cpp)
target_include_directories(system_lab_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(system_lab_core PUBLIC system_lab_options Threads::Threads)

add_executable(system-lab src/main.cpp)
target_link_libraries(system-lab PRIVATE system_lab_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(system_lab_tests tests/system_lab_test.cpp)
    target_link_libraries(system_lab_tests PRIVATE system_lab_core GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(system_lab_tests)
endif()

cmake_minimum_required(VERSION 3.18)

project(CppAppLabW7TcpEcho LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)
find_package(Threads REQUIRED)

add_library(tcp_echo_options INTERFACE)
target_compile_features(tcp_echo_options INTERFACE cxx_std_17)
target_compile_options(tcp_echo_options INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Werror)

add_library(tcp_echo_core src/tcp_echo.cpp)
target_include_directories(tcp_echo_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(tcp_echo_core PUBLIC tcp_echo_options Threads::Threads)

add_executable(tcp-echo src/main.cpp)
target_link_libraries(tcp-echo PRIVATE tcp_echo_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(tcp_echo_tests tests/tcp_echo_test.cpp)
    target_link_libraries(tcp_echo_tests PRIVATE tcp_echo_core GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(tcp_echo_tests)
endif()

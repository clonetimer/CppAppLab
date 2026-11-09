cmake_minimum_required(VERSION 3.18)

project(CppAppLabW7TcpChat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(Threads REQUIRED)

add_library(tcp_chat_options INTERFACE)
target_compile_features(tcp_chat_options INTERFACE cxx_std_17)
target_compile_options(tcp_chat_options INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Werror)

add_library(tcp_chat_core src/tcp_chat.cpp)
target_include_directories(tcp_chat_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(tcp_chat_core PUBLIC tcp_chat_options Threads::Threads)

add_executable(tcp-chat src/main.cpp)
target_link_libraries(tcp-chat PRIVATE tcp_chat_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(tcp_chat_tests tests/tcp_chat_test.cpp)
    target_link_libraries(tcp_chat_tests PRIVATE tcp_chat_core GTest::gtest_main Threads::Threads)
    include(GoogleTest)
    gtest_discover_tests(tcp_chat_tests)
endif()

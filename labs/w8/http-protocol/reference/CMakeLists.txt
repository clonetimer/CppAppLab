cmake_minimum_required(VERSION 3.18)

project(CppAppLabW8HttpProtocol LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_library(http_protocol_options INTERFACE)
target_compile_features(http_protocol_options INTERFACE cxx_std_17)
target_compile_options(http_protocol_options INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Werror)

add_library(http_message_core src/http_message.cpp)
target_include_directories(http_message_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(http_message_core PUBLIC http_protocol_options)

add_executable(http-protocol src/main.cpp)
target_link_libraries(http-protocol PRIVATE http_message_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(http_message_tests tests/http_message_test.cpp)
    target_link_libraries(http_message_tests PRIVATE http_message_core GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(http_message_tests)
endif()

cmake_minimum_required(VERSION 3.18)

project(CppAppLabW8HttpParser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_library(http_parser_options INTERFACE)
target_compile_features(http_parser_options INTERFACE cxx_std_17)
target_compile_options(http_parser_options INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Werror)

add_library(http_request_parser src/http_request_parser.cpp)
target_include_directories(http_request_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(http_request_parser PUBLIC http_parser_options)

add_executable(http-parser src/main.cpp)
target_link_libraries(http-parser PRIVATE http_request_parser)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(http_request_parser_tests tests/http_request_parser_test.cpp)
    target_link_libraries(http_request_parser_tests PRIVATE http_request_parser GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(http_request_parser_tests)
endif()

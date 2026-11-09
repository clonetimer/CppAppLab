cmake_minimum_required(VERSION 3.18)

project(CppAppLabW8StaticFiles LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_library(static_files_options INTERFACE)
target_compile_features(static_files_options INTERFACE cxx_std_17)
target_compile_options(static_files_options INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Werror)

add_library(static_file_handler
    src/http_message.cpp
    src/static_file_handler.cpp
)
target_include_directories(static_file_handler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(static_file_handler PUBLIC static_files_options)

add_executable(http-static-files src/main.cpp)
target_link_libraries(http-static-files PRIVATE static_file_handler)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(static_file_handler_tests tests/static_file_handler_test.cpp)
    target_link_libraries(static_file_handler_tests PRIVATE static_file_handler GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(static_file_handler_tests)
endif()

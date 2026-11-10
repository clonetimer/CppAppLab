cmake_minimum_required(VERSION 3.18)

project(CppAppLabW8Router LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)

add_library(router_core
    src/http_message.cpp
    src/http_request_parser.cpp
    src/static_file_handler.cpp
    src/router.cpp
    src/api_handlers.cpp
    src/dispatcher.cpp
)
target_include_directories(router_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_options(router_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
add_executable(http-router src/main.cpp)
target_link_libraries(http-router PRIVATE router_core)
target_compile_options(http-router PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(router_tests tests/router_test.cpp)
    target_link_libraries(router_tests PRIVATE router_core GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(router_tests PROPERTIES TIMEOUT 15)
endif()

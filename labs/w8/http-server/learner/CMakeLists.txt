cmake_minimum_required(VERSION 3.18)

project(CppAppLabW8HttpServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(Threads REQUIRED)

if(NOT DEFINED CPPAPPLAB_LEARNER_SOURCE_DIR)
    set(CPPAPPLAB_LEARNER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

get_filename_component(CPPAPPLAB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(ROUTER_DIR "${CPPAPPLAB_ROOT}/labs/w8/router/reference")
set(THREAD_POOL_DIR "${CPPAPPLAB_ROOT}/labs/w6/thread-pool/reference")

set(LEARNER_HEADER_DIR "${CPPAPPLAB_LEARNER_SOURCE_DIR}/include")
set(LEARNER_SOURCE_DIR "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src")
set(LEARNER_TEST "${CPPAPPLAB_LEARNER_SOURCE_DIR}/tests/http_server_test.cpp")

foreach(required IN ITEMS
    "${LEARNER_HEADER_DIR}/http_server.hpp"
    "${LEARNER_SOURCE_DIR}/http_server.cpp"
    "${LEARNER_SOURCE_DIR}/main.cpp"
    "${LEARNER_TEST}"
    "${ROUTER_DIR}/src/dispatcher.cpp"
    "${THREAD_POOL_DIR}/src/thread_pool.cpp")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Required HTTP server source is missing: ${required}")
    endif()
endforeach()

add_library(http_server_core
    "${LEARNER_SOURCE_DIR}/http_server.cpp"
    "${THREAD_POOL_DIR}/src/thread_pool.cpp"
    "${ROUTER_DIR}/src/http_message.cpp"
    "${ROUTER_DIR}/src/http_request_parser.cpp"
    "${ROUTER_DIR}/src/static_file_handler.cpp"
    "${ROUTER_DIR}/src/router.cpp"
    "${ROUTER_DIR}/src/api_handlers.cpp"
    "${ROUTER_DIR}/src/dispatcher.cpp"
)
target_include_directories(http_server_core PUBLIC
    "${LEARNER_HEADER_DIR}"
    "${ROUTER_DIR}/include"
    "${THREAD_POOL_DIR}/include"
)
target_compile_options(http_server_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
target_link_libraries(http_server_core PUBLIC Threads::Threads)

add_executable(http-server "${LEARNER_SOURCE_DIR}/main.cpp")
target_link_libraries(http-server PRIVATE http_server_core)
target_compile_options(http-server PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_executable(http_server_tests "${LEARNER_TEST}")
    target_link_libraries(http_server_tests PRIVATE http_server_core GTest::gtest_main)
    target_compile_options(http_server_tests PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
    include(GoogleTest)
    gtest_discover_tests(http_server_tests PROPERTIES TIMEOUT 15)
endif()

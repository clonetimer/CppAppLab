cmake_minimum_required(VERSION 3.18)

project(CppAppLabW8GracefulShutdownAcceptance LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(GTest CONFIG REQUIRED)
find_package(Threads REQUIRED)

get_filename_component(CPPAPPLAB_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
if(NOT DEFINED CPPAPPLAB_ROUTER_SOURCE_DIR)
    set(CPPAPPLAB_ROUTER_SOURCE_DIR "${CPPAPPLAB_REPOSITORY_ROOT}/labs/w8/router/reference")
endif()
if(NOT DEFINED CPPAPPLAB_THREAD_POOL_SOURCE_DIR)
    set(CPPAPPLAB_THREAD_POOL_SOURCE_DIR "${CPPAPPLAB_REPOSITORY_ROOT}/labs/w6/thread-pool/reference")
endif()

if(DEFINED CPPAPPLAB_LEARNER_SOURCE_DIR)
    set(HTTP_SERVER_SOURCE_DIR "${CPPAPPLAB_LEARNER_SOURCE_DIR}")
elseif(DEFINED CPPAPPLAB_HTTP_SERVER_SOURCE_DIR)
    set(HTTP_SERVER_SOURCE_DIR "${CPPAPPLAB_HTTP_SERVER_SOURCE_DIR}")
else()
    message(FATAL_ERROR "CPPAPPLAB_LEARNER_SOURCE_DIR or CPPAPPLAB_HTTP_SERVER_SOURCE_DIR is required.")
endif()

set(HTTP_SERVER_SOURCE "${HTTP_SERVER_SOURCE_DIR}/src/http_server.cpp")
set(ROUTER_SOURCES
    "${CPPAPPLAB_ROUTER_SOURCE_DIR}/src/http_message.cpp"
    "${CPPAPPLAB_ROUTER_SOURCE_DIR}/src/http_request_parser.cpp"
    "${CPPAPPLAB_ROUTER_SOURCE_DIR}/src/static_file_handler.cpp"
    "${CPPAPPLAB_ROUTER_SOURCE_DIR}/src/router.cpp"
    "${CPPAPPLAB_ROUTER_SOURCE_DIR}/src/api_handlers.cpp"
    "${CPPAPPLAB_ROUTER_SOURCE_DIR}/src/dispatcher.cpp"
)
set(THREAD_POOL_SOURCE "${CPPAPPLAB_THREAD_POOL_SOURCE_DIR}/src/thread_pool.cpp")

foreach(required IN ITEMS
    "${HTTP_SERVER_SOURCE_DIR}/include/http_server.hpp"
    "${HTTP_SERVER_SOURCE}"
    "${CPPAPPLAB_ROUTER_SOURCE_DIR}/include/http_message.hpp"
    "${CPPAPPLAB_ROUTER_SOURCE_DIR}/include/http_request_parser.hpp"
    "${CPPAPPLAB_ROUTER_SOURCE_DIR}/include/dispatcher.hpp"
    "${CPPAPPLAB_THREAD_POOL_SOURCE_DIR}/include/thread_pool.hpp"
    "${THREAD_POOL_SOURCE}"
    ${ROUTER_SOURCES})
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Required HTTP server source is missing: ${required}")
    endif()
endforeach()

add_library(http_graceful_shutdown_core
    "${HTTP_SERVER_SOURCE}"
    "${THREAD_POOL_SOURCE}"
    ${ROUTER_SOURCES}
)
target_include_directories(http_graceful_shutdown_core PUBLIC
    "${HTTP_SERVER_SOURCE_DIR}/include"
    "${CPPAPPLAB_ROUTER_SOURCE_DIR}/include"
    "${CPPAPPLAB_THREAD_POOL_SOURCE_DIR}/include"
)
target_compile_options(http_graceful_shutdown_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
target_link_libraries(http_graceful_shutdown_core PUBLIC Threads::Threads)

add_executable(http_graceful_shutdown_acceptance_tests tests/http_graceful_shutdown_acceptance_test.cpp)
target_link_libraries(http_graceful_shutdown_acceptance_tests PRIVATE
    http_graceful_shutdown_core
    GTest::gtest_main
)
target_compile_options(http_graceful_shutdown_acceptance_tests PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Werror
)

include(GoogleTest)
gtest_discover_tests(http_graceful_shutdown_acceptance_tests PROPERTIES TIMEOUT 10)

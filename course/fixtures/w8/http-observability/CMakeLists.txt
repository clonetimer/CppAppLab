cmake_minimum_required(VERSION 3.18)

project(CppAppLabW8HttpObservabilityAcceptance LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(GTest CONFIG REQUIRED)
find_package(Threads REQUIRED)

foreach(variable IN ITEMS
    CPPAPPLAB_HTTP_SERVER_SOURCE_DIR
    CPPAPPLAB_ROUTER_SOURCE_DIR
    CPPAPPLAB_THREAD_POOL_SOURCE_DIR)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "${variable} is required.")
    endif()
endforeach()

set(HTTP_SERVER_SOURCE "${CPPAPPLAB_HTTP_SERVER_SOURCE_DIR}/src/http_server.cpp")
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
    "${CPPAPPLAB_HTTP_SERVER_SOURCE_DIR}/include/http_server.hpp"
    "${HTTP_SERVER_SOURCE}"
    "${CPPAPPLAB_ROUTER_SOURCE_DIR}/include/http_message.hpp"
    "${CPPAPPLAB_ROUTER_SOURCE_DIR}/include/http_request_parser.hpp"
    "${CPPAPPLAB_ROUTER_SOURCE_DIR}/include/dispatcher.hpp"
    "${CPPAPPLAB_THREAD_POOL_SOURCE_DIR}/include/thread_pool.hpp"
    "${THREAD_POOL_SOURCE}"
    ${ROUTER_SOURCES})
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Required HTTP observability source is missing: ${required}")
    endif()
endforeach()

add_library(http_observability_core
    "${HTTP_SERVER_SOURCE}"
    "${THREAD_POOL_SOURCE}"
    ${ROUTER_SOURCES}
)
target_include_directories(http_observability_core PUBLIC
    "${CPPAPPLAB_HTTP_SERVER_SOURCE_DIR}/include"
    "${CPPAPPLAB_ROUTER_SOURCE_DIR}/include"
    "${CPPAPPLAB_THREAD_POOL_SOURCE_DIR}/include"
)
target_compile_options(http_observability_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
target_link_libraries(http_observability_core PUBLIC Threads::Threads)

add_executable(http_observability_acceptance_tests tests/http_observability_acceptance_test.cpp)
target_link_libraries(http_observability_acceptance_tests PRIVATE
    http_observability_core
    GTest::gtest_main
)
target_compile_options(http_observability_acceptance_tests PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Werror
)

include(GoogleTest)
gtest_discover_tests(http_observability_acceptance_tests PROPERTIES TIMEOUT 10)

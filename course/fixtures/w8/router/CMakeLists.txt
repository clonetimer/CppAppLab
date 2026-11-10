cmake_minimum_required(VERSION 3.18)

project(CppAppLabW8RouterAcceptance LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)

if(NOT DEFINED CPPAPPLAB_LEARNER_SOURCE_DIR)
    message(FATAL_ERROR "CPPAPPLAB_LEARNER_SOURCE_DIR is required.")
endif()
find_package(GTest CONFIG REQUIRED)
set(learner "${CPPAPPLAB_LEARNER_SOURCE_DIR}")
add_library(router_under_test STATIC
    "${learner}/src/http_message.cpp"
    "${learner}/src/http_request_parser.cpp"
    "${learner}/src/static_file_handler.cpp"
    "${learner}/src/router.cpp"
    "${learner}/src/api_handlers.cpp"
    "${learner}/src/dispatcher.cpp"
)
target_include_directories(router_under_test PUBLIC "${learner}/include")
target_compile_options(router_under_test PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
add_executable(http-router "${learner}/src/main.cpp")
target_link_libraries(http-router PRIVATE router_under_test)
target_compile_options(http-router PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)

if(BUILD_TESTING)
    add_executable(router_acceptance_tests tests/router_acceptance_test.cpp)
    target_include_directories(router_acceptance_tests BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(router_acceptance_tests PRIVATE router_under_test GTest::gtest_main)
    target_compile_options(router_acceptance_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_executable(router_learner_tests "${learner}/tests/router_test.cpp")
    target_link_libraries(router_learner_tests PRIVATE router_under_test GTest::gtest_main)
    target_compile_options(router_learner_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    include(GoogleTest)
    gtest_discover_tests(router_acceptance_tests PROPERTIES TIMEOUT 15)
    gtest_discover_tests(router_learner_tests PROPERTIES TIMEOUT 15)
    add_test(NAME router_cli_smoke
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/router_smoke.sh" "$<TARGET_FILE:http-router>")
    set_tests_properties(router_cli_smoke PROPERTIES TIMEOUT 15)
endif()

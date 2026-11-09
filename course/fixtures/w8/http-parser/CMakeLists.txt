cmake_minimum_required(VERSION 3.18)

project(CppAppLabW8HttpParserAcceptance LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

if(NOT DEFINED CPPAPPLAB_LEARNER_SOURCE_DIR)
    message(FATAL_ERROR "CPPAPPLAB_LEARNER_SOURCE_DIR is required.")
endif()

set(PARSER_SOURCE "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src/http_request_parser.cpp")
set(MAIN_SOURCE "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src/main.cpp")
set(LEARNER_TEST "${CPPAPPLAB_LEARNER_SOURCE_DIR}/tests/http_request_parser_test.cpp")

foreach(file IN ITEMS "${PARSER_SOURCE}" "${MAIN_SOURCE}" "${LEARNER_TEST}")
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "Required learner source is missing: ${file}")
    endif()
endforeach()

find_package(GTest CONFIG REQUIRED)

add_library(http_parser_under_test STATIC "${PARSER_SOURCE}")
target_include_directories(http_parser_under_test PUBLIC "${CPPAPPLAB_LEARNER_SOURCE_DIR}/include")
target_compile_options(http_parser_under_test PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(http-parser "${MAIN_SOURCE}")
target_link_libraries(http-parser PRIVATE http_parser_under_test)
target_compile_options(http-parser PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(BUILD_TESTING)
    add_executable(http_parser_acceptance_tests tests/http_request_parser_acceptance_test.cpp)
    target_include_directories(http_parser_acceptance_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(http_parser_acceptance_tests PRIVATE http_parser_under_test GTest::gtest_main)
    target_compile_options(http_parser_acceptance_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)

    add_executable(http_parser_learner_tests "${LEARNER_TEST}")
    target_link_libraries(http_parser_learner_tests PRIVATE http_parser_under_test GTest::gtest_main)
    target_compile_options(http_parser_learner_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)

    include(GoogleTest)
    gtest_discover_tests(http_parser_acceptance_tests)
    gtest_discover_tests(http_parser_learner_tests)

    add_test(
        NAME http_parser_cli_smoke
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/http_parser_smoke.sh" "$<TARGET_FILE:http-parser>"
    )
    set_tests_properties(http_parser_cli_smoke PROPERTIES TIMEOUT 10)
endif()

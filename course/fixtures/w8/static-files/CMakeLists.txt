cmake_minimum_required(VERSION 3.18)

project(CppAppLabW8StaticFilesAcceptance LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

if(NOT DEFINED CPPAPPLAB_LEARNER_SOURCE_DIR)
    message(FATAL_ERROR "CPPAPPLAB_LEARNER_SOURCE_DIR is required.")
endif()

set(MESSAGE_SOURCE "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src/http_message.cpp")
set(HANDLER_SOURCE "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src/static_file_handler.cpp")
set(MAIN_SOURCE "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src/main.cpp")
set(LEARNER_TEST "${CPPAPPLAB_LEARNER_SOURCE_DIR}/tests/static_file_handler_test.cpp")

foreach(file IN ITEMS "${MESSAGE_SOURCE}" "${HANDLER_SOURCE}" "${MAIN_SOURCE}" "${LEARNER_TEST}")
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "Required learner source is missing: ${file}")
    endif()
endforeach()

find_package(GTest CONFIG REQUIRED)

add_library(static_files_under_test STATIC "${MESSAGE_SOURCE}" "${HANDLER_SOURCE}")
target_include_directories(static_files_under_test PUBLIC "${CPPAPPLAB_LEARNER_SOURCE_DIR}/include")
target_compile_options(static_files_under_test PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(http-static-files "${MAIN_SOURCE}")
target_link_libraries(http-static-files PRIVATE static_files_under_test)
target_compile_options(http-static-files PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(BUILD_TESTING)
    add_executable(static_file_acceptance_tests tests/static_file_acceptance_test.cpp)
    target_include_directories(static_file_acceptance_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(static_file_acceptance_tests PRIVATE static_files_under_test GTest::gtest_main)
    target_compile_options(static_file_acceptance_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)

    add_executable(static_file_learner_tests "${LEARNER_TEST}")
    target_link_libraries(static_file_learner_tests PRIVATE static_files_under_test GTest::gtest_main)
    target_compile_options(static_file_learner_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)

    include(GoogleTest)
    gtest_discover_tests(static_file_acceptance_tests)
    gtest_discover_tests(static_file_learner_tests)

    add_test(
        NAME static_files_cli_smoke
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/static_files_smoke.sh" "$<TARGET_FILE:http-static-files>"
    )
    set_tests_properties(static_files_cli_smoke PROPERTIES TIMEOUT 10)
endif()

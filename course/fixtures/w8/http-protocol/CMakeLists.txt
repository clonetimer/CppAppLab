cmake_minimum_required(VERSION 3.18)

project(CppAppLabW8HttpProtocolAcceptance LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

if(NOT DEFINED CPPAPPLAB_LEARNER_SOURCE_DIR)
    message(FATAL_ERROR "CPPAPPLAB_LEARNER_SOURCE_DIR is required.")
endif()

set(HTTP_SOURCE "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src/http_message.cpp")
set(MAIN_SOURCE "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src/main.cpp")
set(LEARNER_TEST "${CPPAPPLAB_LEARNER_SOURCE_DIR}/tests/http_message_test.cpp")

foreach(file IN ITEMS "${HTTP_SOURCE}" "${MAIN_SOURCE}" "${LEARNER_TEST}")
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "Required learner source is missing: ${file}")
    endif()
endforeach()

find_package(GTest CONFIG REQUIRED)

add_library(http_message_under_test STATIC "${HTTP_SOURCE}")
target_include_directories(http_message_under_test PUBLIC "${CPPAPPLAB_LEARNER_SOURCE_DIR}/include")
target_compile_options(http_message_under_test PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(http-protocol "${MAIN_SOURCE}")
target_link_libraries(http-protocol PRIVATE http_message_under_test)
target_compile_options(http-protocol PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(BUILD_TESTING)
    add_executable(http_protocol_acceptance_tests tests/http_protocol_acceptance_test.cpp)
    target_include_directories(http_protocol_acceptance_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(http_protocol_acceptance_tests PRIVATE http_message_under_test GTest::gtest_main)
    target_compile_options(http_protocol_acceptance_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)

    add_executable(http_protocol_learner_tests "${LEARNER_TEST}")
    target_link_libraries(http_protocol_learner_tests PRIVATE http_message_under_test GTest::gtest_main)
    target_compile_options(http_protocol_learner_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)

    include(GoogleTest)
    gtest_discover_tests(http_protocol_acceptance_tests)
    gtest_discover_tests(http_protocol_learner_tests)

    add_test(
        NAME http_protocol_cli_smoke
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/http_protocol_smoke.sh" "$<TARGET_FILE:http-protocol>"
    )
    set_tests_properties(http_protocol_cli_smoke PROPERTIES TIMEOUT 10)
endif()

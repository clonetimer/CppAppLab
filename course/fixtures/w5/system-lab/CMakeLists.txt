cmake_minimum_required(VERSION 3.18)

project(CppAppLabW5SystemProgrammingAcceptance LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

if(NOT DEFINED CPPAPPLAB_LEARNER_SOURCE_DIR)
    message(FATAL_ERROR "CPPAPPLAB_LEARNER_SOURCE_DIR is required.")
endif()

set(QUEUE_SOURCE "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src/process_runner.cpp")
set(MAIN_SOURCE "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src/main.cpp")
set(LEARNER_TEST_SOURCE "${CPPAPPLAB_LEARNER_SOURCE_DIR}/tests/system_lab_test.cpp")

foreach(required_file IN ITEMS "${QUEUE_SOURCE}" "${MAIN_SOURCE}" "${LEARNER_TEST_SOURCE}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Required learner source is missing: ${required_file}")
    endif()
endforeach()

find_package(GTest CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_library(system_lab_under_test STATIC "${QUEUE_SOURCE}")
target_include_directories(system_lab_under_test PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(system_lab_under_test PUBLIC Threads::Threads)
target_compile_options(system_lab_under_test PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(system-lab "${MAIN_SOURCE}")
target_link_libraries(system-lab PRIVATE system_lab_under_test)
target_compile_options(system-lab PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(BUILD_TESTING)
    add_executable(system_lab_acceptance_tests tests/system_lab_acceptance_test.cpp)
    target_link_libraries(system_lab_acceptance_tests PRIVATE system_lab_under_test GTest::gtest_main Threads::Threads)
    target_compile_options(system_lab_acceptance_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)

    add_executable(system_lab_learner_tests "${LEARNER_TEST_SOURCE}")
    target_link_libraries(system_lab_learner_tests PRIVATE system_lab_under_test GTest::gtest_main Threads::Threads)
    target_compile_options(system_lab_learner_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)

    include(GoogleTest)
    gtest_discover_tests(system_lab_acceptance_tests)
    gtest_discover_tests(system_lab_learner_tests)

    add_test(
        NAME system_lab_cli_smoke
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/system_lab_cli_smoke.sh" "$<TARGET_FILE:system-lab>"
    )
    set_tests_properties(system_lab_cli_smoke PROPERTIES TIMEOUT 10)
endif()

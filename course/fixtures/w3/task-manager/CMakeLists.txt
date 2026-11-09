cmake_minimum_required(VERSION 3.18)

project(CppAppLabW3TaskManagerAcceptance LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

if(NOT DEFINED CPPAPPLAB_LEARNER_SOURCE_DIR)
    message(FATAL_ERROR "CPPAPPLAB_LEARNER_SOURCE_DIR is required.")
endif()

set(LEARNER_TASK_SOURCE
    "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src/task.cpp"
)
set(LEARNER_MANAGER_SOURCE
    "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src/task_manager.cpp"
)
set(LEARNER_MAIN_SOURCE
    "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src/main.cpp"
)

foreach(required_file IN ITEMS
        "${LEARNER_TASK_SOURCE}"
        "${LEARNER_MANAGER_SOURCE}"
        "${LEARNER_MAIN_SOURCE}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Required learner source is missing: ${required_file}")
    endif()
endforeach()

find_package(GTest CONFIG REQUIRED)

add_library(
    task_manager_under_test
    STATIC
    "${LEARNER_TASK_SOURCE}"
    "${LEARNER_MANAGER_SOURCE}"
)
target_include_directories(
    task_manager_under_test
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_options(
    task_manager_under_test
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_executable(
    todo_cli
    "${LEARNER_MAIN_SOURCE}"
    "${LEARNER_TASK_SOURCE}"
    "${LEARNER_MANAGER_SOURCE}"
)
target_include_directories(
    todo_cli
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_options(
    todo_cli
    PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

if(BUILD_TESTING)
    add_executable(
        task_manager_acceptance_tests
        tests/task_manager_acceptance_test.cpp
    )
    target_link_libraries(
        task_manager_acceptance_tests
        PRIVATE
        task_manager_under_test
        GTest::gtest_main
    )
    target_compile_options(
        task_manager_acceptance_tests
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )

    include(GoogleTest)
    gtest_discover_tests(task_manager_acceptance_tests)

    add_test(
        NAME task_manager_cli_workflow
        COMMAND bash
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/task_manager_cli_workflow.sh"
            "$<TARGET_FILE:todo_cli>"
    )
    set_tests_properties(task_manager_cli_workflow PROPERTIES TIMEOUT 15)
endif()

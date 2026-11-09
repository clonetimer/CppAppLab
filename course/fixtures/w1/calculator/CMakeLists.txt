cmake_minimum_required(VERSION 3.18)

project(CppAppLabW1CalculatorAcceptance LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

if(NOT DEFINED CPPAPPLAB_LEARNER_SOURCE_DIR)
    message(FATAL_ERROR "CPPAPPLAB_LEARNER_SOURCE_DIR is required.")
endif()

set(LEARNER_CALCULATOR_SOURCE
    "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src/calculator.cpp"
)
set(LEARNER_MAIN_SOURCE
    "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src/main.cpp"
)

foreach(required_file IN ITEMS "${LEARNER_CALCULATOR_SOURCE}" "${LEARNER_MAIN_SOURCE}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Required learner source is missing: ${required_file}")
    endif()
endforeach()

find_package(GTest CONFIG REQUIRED)

add_library(learner_calculator STATIC "${LEARNER_CALCULATOR_SOURCE}")
target_include_directories(
    learner_calculator
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_options(learner_calculator PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(
    calculator_cli
    "${LEARNER_MAIN_SOURCE}"
    "${LEARNER_CALCULATOR_SOURCE}"
)
target_include_directories(
    calculator_cli
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_options(calculator_cli PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(BUILD_TESTING)
    add_executable(
        calculator_acceptance_tests
        tests/calculator_acceptance_test.cpp
    )
    target_include_directories(
        calculator_acceptance_tests
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    target_link_libraries(
        calculator_acceptance_tests
        PRIVATE
        learner_calculator
        GTest::gtest_main
    )
    target_compile_options(
        calculator_acceptance_tests
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )

    include(GoogleTest)
    gtest_discover_tests(calculator_acceptance_tests)
endif()

cmake_minimum_required(VERSION 3.18)

project(CppAppLabW4QualityGate LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

option(CPPAPPLAB_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(CPPAPPLAB_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(CPPAPPLAB_ENABLE_CLANG_TIDY "Run clang-tidy during compilation" OFF)

if(CPPAPPLAB_ENABLE_CLANG_TIDY)
    find_program(CPPAPPLAB_CLANG_TIDY NAMES clang-tidy REQUIRED)
    set(CMAKE_CXX_CLANG_TIDY "${CPPAPPLAB_CLANG_TIDY}")
endif()

add_library(cppapplab_quality_options INTERFACE)
target_compile_features(cppapplab_quality_options INTERFACE cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        cppapplab_quality_options
        INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
    )
    if(CPPAPPLAB_WARNINGS_AS_ERRORS)
        target_compile_options(cppapplab_quality_options INTERFACE -Werror)
    endif()
else()
    message(FATAL_ERROR "This W4 exercise expects GCC or Clang on Linux.")
endif()

if(CPPAPPLAB_ENABLE_SANITIZERS)
    target_compile_options(
        cppapplab_quality_options
        INTERFACE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(
        cppapplab_quality_options
        INTERFACE
        -fsanitize=address,undefined
    )
endif()

add_library(
    todo_core
    src/task.cpp
    src/task_manager.cpp
)
target_include_directories(todo_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(todo_core PUBLIC cppapplab_quality_options)

add_executable(todo src/main.cpp)
target_link_libraries(todo PRIVATE todo_core cppapplab_quality_options)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)

    add_executable(task_manager_tests tests/task_manager_test.cpp)
    target_link_libraries(
        task_manager_tests
        PRIVATE
        todo_core
        GTest::gtest_main
        cppapplab_quality_options
    )

    include(GoogleTest)
    gtest_discover_tests(task_manager_tests)
endif()

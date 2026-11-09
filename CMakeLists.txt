cmake_minimum_required(VERSION 3.18)

project(
    CppAppLab
    VERSION 0.1.0
    DESCRIPTION "A hands-on C++ learning and engineering-practice platform"
    LANGUAGES CXX
)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "CppAppLab v1 learning engine requires Linux; use WSL2 + Ubuntu 24.04.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

option(CPPAPPLAB_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(CPPAPPLAB_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(CPPAPPLAB_ENABLE_CLANG_TIDY "Run clang-tidy during compilation" OFF)
option(
    CPPAPPLAB_FETCH_GTEST
    "Fetch the pinned GoogleTest version when no system package is available"
    ON
)

if(CPPAPPLAB_ENABLE_CLANG_TIDY)
    find_program(CPPAPPLAB_CLANG_TIDY NAMES clang-tidy REQUIRED)
    set(CMAKE_CXX_CLANG_TIDY "${CPPAPPLAB_CLANG_TIDY}")
endif()

add_library(cppapplab_build_options INTERFACE)
target_compile_features(cppapplab_build_options INTERFACE cxx_std_17)

if(MSVC)
    target_compile_options(cppapplab_build_options INTERFACE /W4 /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(
        cppapplab_build_options
        INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
    )
endif()

if(CPPAPPLAB_WARNINGS_AS_ERRORS)
    if(MSVC)
        target_compile_options(cppapplab_build_options INTERFACE /WX)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(cppapplab_build_options INTERFACE -Werror)
    endif()
endif()

if(CPPAPPLAB_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
            cppapplab_build_options
            INTERFACE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(cppapplab_build_options INTERFACE -fsanitize=address,undefined)
    else()
        message(FATAL_ERROR "CPPAPPLAB_ENABLE_SANITIZERS currently requires GCC or Clang.")
    endif()
endif()

add_library(
    cppapplab_core
    src/app.cpp
    src/task.cpp
    src/workspace.cpp
    src/progress.cpp
    src/process.cpp
    src/learning.cpp
)
target_include_directories(
    cppapplab_core
    PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
target_compile_definitions(
    cppapplab_core
    PUBLIC
    CPPAPPLAB_VERSION="${PROJECT_VERSION}"
    PRIVATE
    CPPAPPLAB_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(cppapplab_core PUBLIC cppapplab_build_options)

add_executable(cppapplab src/main.cpp)
target_link_libraries(cppapplab PRIVATE cppapplab_core)

if(BUILD_TESTING)
    find_package(GTest CONFIG QUIET)

    if(NOT GTest_FOUND)
        if(CPPAPPLAB_FETCH_GTEST)
            include(FetchContent)
            set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
            set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
            FetchContent_Declare(
                googletest
                GIT_REPOSITORY https://github.com/google/googletest.git
                GIT_TAG v1.18.0
                GIT_SHALLOW TRUE
            )
            FetchContent_MakeAvailable(googletest)
        else()
            message(FATAL_ERROR
                "GoogleTest was not found. Install libgtest-dev or enable CPPAPPLAB_FETCH_GTEST."
            )
        endif()
    endif()

    add_executable(
        cppapplab_tests
        tests/smoke_test.cpp
        tests/catalog_test.cpp
        tests/progress_test.cpp
        tests/process_test.cpp
        tests/workspace_test.cpp
        tests/learning_workflow_test.cpp
        tests/calculator_workflow_test.cpp
    )
    target_link_libraries(cppapplab_tests PRIVATE cppapplab_core GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(cppapplab_tests)
endif()

cmake_minimum_required(VERSION 3.18)

project(CppAppLabNativeProcessAcceptance LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(GTest CONFIG REQUIRED)

if(WIN32)
    set(CPPAPPLAB_PROCESS_IMPLEMENTATION "${CMAKE_CURRENT_LIST_DIR}/../../../src/process_win32.cpp")
else()
    set(CPPAPPLAB_PROCESS_IMPLEMENTATION "${CMAKE_CURRENT_LIST_DIR}/../../../src/process.cpp")
endif()

add_library(cppapplab_process_backend STATIC "${CPPAPPLAB_PROCESS_IMPLEMENTATION}")
target_include_directories(cppapplab_process_backend PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/../../../include"
)
target_compile_features(cppapplab_process_backend PUBLIC cxx_std_17)

add_executable(cppapplab_process_test_child
    "${CMAKE_CURRENT_LIST_DIR}/../../process_test_child.cpp"
)
add_executable(cppapplab_process_tests
    "${CMAKE_CURRENT_LIST_DIR}/../../process_test_main.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../../process_test.cpp"
)
target_link_libraries(cppapplab_process_tests
    PRIVATE
    cppapplab_process_backend
    GTest::gtest
)

if(MSVC)
    target_compile_options(cppapplab_process_backend PRIVATE /W4 /WX)
    target_compile_options(cppapplab_process_test_child PRIVATE /W4 /WX)
    target_compile_options(cppapplab_process_tests PRIVATE /W4 /WX)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cppapplab_process_backend PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(cppapplab_process_test_child PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(cppapplab_process_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_dependencies(cppapplab_process_tests cppapplab_process_test_child)

if(BUILD_TESTING)
    include(GoogleTest)
    gtest_discover_tests(cppapplab_process_tests PROPERTIES TIMEOUT 10)
endif()

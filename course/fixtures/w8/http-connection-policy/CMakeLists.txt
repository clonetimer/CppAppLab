cmake_minimum_required(VERSION 3.18)

project(CppAppLabW8HttpConnectionPolicyAcceptance LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(GTest CONFIG REQUIRED)

if(DEFINED CPPAPPLAB_LEARNER_SOURCE_DIR)
    set(LEARNER_HEADER "${CPPAPPLAB_LEARNER_SOURCE_DIR}/include/http_message.hpp")
    set(LEARNER_SOURCE "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src/http_message.cpp")
    foreach(required IN ITEMS "${LEARNER_HEADER}" "${LEARNER_SOURCE}")
        if(NOT EXISTS "${required}")
            message(FATAL_ERROR "Required learner HTTP message source is missing: ${required}")
        endif()
    endforeach()

    add_executable(http_connection_policy_learner_acceptance_tests
        tests/connection_policy_acceptance_test.cpp
        "${LEARNER_SOURCE}"
    )
    target_include_directories(http_connection_policy_learner_acceptance_tests PRIVATE
        "${CPPAPPLAB_LEARNER_SOURCE_DIR}/include"
    )
    target_compile_options(http_connection_policy_learner_acceptance_tests PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Werror
    )
    target_link_libraries(http_connection_policy_learner_acceptance_tests PRIVATE GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(http_connection_policy_learner_acceptance_tests PROPERTIES TIMEOUT 10)
else()
    foreach(variable IN ITEMS CPPAPPLAB_HTTP_PROTOCOL_SOURCE_DIR CPPAPPLAB_ROUTER_SOURCE_DIR)
        if(NOT DEFINED ${variable})
            message(FATAL_ERROR "${variable} is required.")
        endif()
    endforeach()

    function(add_connection_policy_fixture target source_dir)
        set(header "${source_dir}/include/http_message.hpp")
        set(source "${source_dir}/src/http_message.cpp")
        if(NOT EXISTS "${header}" OR NOT EXISTS "${source}")
            message(FATAL_ERROR "HTTP message source is incomplete: ${source_dir}")
        endif()

        add_executable(${target}
            tests/connection_policy_acceptance_test.cpp
            "${source}"
        )
        target_include_directories(${target} PRIVATE "${source_dir}/include")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
        target_link_libraries(${target} PRIVATE GTest::gtest_main)

        include(GoogleTest)
        gtest_discover_tests(${target} PROPERTIES TIMEOUT 10)
    endfunction()

    add_connection_policy_fixture(http_protocol_connection_policy_tests
        "${CPPAPPLAB_HTTP_PROTOCOL_SOURCE_DIR}")
    add_connection_policy_fixture(router_connection_policy_tests
        "${CPPAPPLAB_ROUTER_SOURCE_DIR}")
endif()

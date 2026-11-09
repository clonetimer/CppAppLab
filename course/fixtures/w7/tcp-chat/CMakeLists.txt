cmake_minimum_required(VERSION 3.18)

project(CppAppLabW7TcpChatAcceptance LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

if(NOT DEFINED CPPAPPLAB_LEARNER_SOURCE_DIR)
    message(FATAL_ERROR "CPPAPPLAB_LEARNER_SOURCE_DIR is required.")
endif()

set(CHAT_SOURCE "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src/tcp_chat.cpp")
set(MAIN_SOURCE "${CPPAPPLAB_LEARNER_SOURCE_DIR}/src/main.cpp")
set(LEARNER_TEST "${CPPAPPLAB_LEARNER_SOURCE_DIR}/tests/tcp_chat_test.cpp")

foreach(file IN ITEMS "${CHAT_SOURCE}" "${MAIN_SOURCE}" "${LEARNER_TEST}")
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "Required learner source is missing: ${file}")
    endif()
endforeach()

find_package(GTest CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_library(tcp_chat_under_test STATIC "${CHAT_SOURCE}")
target_include_directories(tcp_chat_under_test PUBLIC "${CPPAPPLAB_LEARNER_SOURCE_DIR}/include")
target_link_libraries(tcp_chat_under_test PUBLIC Threads::Threads)
target_compile_options(tcp_chat_under_test PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(tcp-chat "${MAIN_SOURCE}")
target_link_libraries(tcp-chat PRIVATE tcp_chat_under_test)
target_compile_options(tcp-chat PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(BUILD_TESTING)
    add_executable(tcp_chat_acceptance_tests tests/tcp_chat_acceptance_test.cpp)
    target_include_directories(tcp_chat_acceptance_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(tcp_chat_acceptance_tests PRIVATE tcp_chat_under_test GTest::gtest_main Threads::Threads)
    target_compile_options(tcp_chat_acceptance_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)

    add_executable(tcp_chat_learner_tests "${LEARNER_TEST}")
    target_link_libraries(tcp_chat_learner_tests PRIVATE tcp_chat_under_test GTest::gtest_main Threads::Threads)
    target_compile_options(tcp_chat_learner_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)

    include(GoogleTest)
    gtest_discover_tests(tcp_chat_acceptance_tests)
    gtest_discover_tests(tcp_chat_learner_tests)

    add_test(
        NAME tcp_chat_cli_smoke
        COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/tcp_chat_smoke.sh" "$<TARGET_FILE:tcp-chat>"
    )
    set_tests_properties(tcp_chat_cli_smoke PROPERTIES TIMEOUT 10)
endif()

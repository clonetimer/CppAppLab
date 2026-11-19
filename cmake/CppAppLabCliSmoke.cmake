include(CMakeParseArguments)

function(cppapplab_add_cli_smoke)
    cmake_parse_arguments(
        SMOKE
        ""
        "NAME;TARGET;TIMEOUT"
        "ARGUMENTS;EXPECTED_LINES;EXPECTED_SUBSTRINGS"
        ${ARGN}
    )

    if(SMOKE_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown CLI smoke arguments: ${SMOKE_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT SMOKE_NAME OR NOT SMOKE_TARGET)
        message(FATAL_ERROR "cppapplab_add_cli_smoke requires NAME and TARGET.")
    endif()
    if(NOT TARGET "${SMOKE_TARGET}")
        message(FATAL_ERROR "CLI smoke target does not exist: ${SMOKE_TARGET}")
    endif()
    if(NOT SMOKE_TIMEOUT)
        set(SMOKE_TIMEOUT 10)
    endif()

    set(test_command
        "${CMAKE_COMMAND}"
        "-DCPPAPPLAB_EXECUTABLE=$<TARGET_FILE:${SMOKE_TARGET}>"
        "-DCPPAPPLAB_TIMEOUT_SECONDS=5"
    )
    set(index 0)
    foreach(argument IN LISTS SMOKE_ARGUMENTS)
        math(EXPR index "${index} + 1")
        list(APPEND test_command "-DCPPAPPLAB_ARGUMENT_${index}=${argument}")
    endforeach()
    set(index 0)
    foreach(expected IN LISTS SMOKE_EXPECTED_LINES)
        math(EXPR index "${index} + 1")
        list(APPEND test_command "-DCPPAPPLAB_EXPECTED_LINE_${index}=${expected}")
    endforeach()
    set(index 0)
    foreach(expected IN LISTS SMOKE_EXPECTED_SUBSTRINGS)
        math(EXPR index "${index} + 1")
        list(APPEND test_command "-DCPPAPPLAB_EXPECTED_SUBSTRING_${index}=${expected}")
    endforeach()

    list(APPEND test_command -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/AssertCliSmoke.cmake")
    add_test(NAME "${SMOKE_NAME}" COMMAND ${test_command})
    set_tests_properties("${SMOKE_NAME}" PROPERTIES TIMEOUT "${SMOKE_TIMEOUT}")
endfunction()

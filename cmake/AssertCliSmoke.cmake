if(NOT DEFINED CPPAPPLAB_EXECUTABLE OR CPPAPPLAB_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR "CPPAPPLAB_EXECUTABLE is required.")
endif()
if(NOT DEFINED CPPAPPLAB_TIMEOUT_SECONDS OR CPPAPPLAB_TIMEOUT_SECONDS STREQUAL "")
    set(CPPAPPLAB_TIMEOUT_SECONDS 5)
endif()

set(command "${CPPAPPLAB_EXECUTABLE}")
foreach(index RANGE 1 16)
    if(DEFINED CPPAPPLAB_ARGUMENT_${index})
        list(APPEND command "${CPPAPPLAB_ARGUMENT_${index}}")
    endif()
endforeach()

execute_process(
    COMMAND ${command}
    RESULT_VARIABLE actual_exit
    OUTPUT_VARIABLE actual_stdout
    ERROR_VARIABLE actual_stderr
    TIMEOUT "${CPPAPPLAB_TIMEOUT_SECONDS}"
)
if(NOT "${actual_exit}" STREQUAL "0")
    message(FATAL_ERROR
        "CLI smoke command exited with '${actual_exit}'.\n"
        "stdout: ${actual_stdout}\n"
        "stderr: ${actual_stderr}"
    )
endif()

string(REPLACE "\r\n" "\n" normalized_stdout "${actual_stdout}")
set(padded_stdout "\n${normalized_stdout}\n")
set(expectation_count 0)
foreach(index RANGE 1 16)
    if(DEFINED CPPAPPLAB_EXPECTED_LINE_${index})
        math(EXPR expectation_count "${expectation_count} + 1")
        set(expected "${CPPAPPLAB_EXPECTED_LINE_${index}}")
        string(FIND "${padded_stdout}" "\n${expected}\n" position)
        if(position EQUAL -1)
            message(FATAL_ERROR
                "Expected exact output line not found: '${expected}'.\n"
                "Actual stdout:\n${normalized_stdout}"
            )
        endif()
    endif()
    if(DEFINED CPPAPPLAB_EXPECTED_SUBSTRING_${index})
        math(EXPR expectation_count "${expectation_count} + 1")
        set(expected "${CPPAPPLAB_EXPECTED_SUBSTRING_${index}}")
        string(FIND "${normalized_stdout}" "${expected}" position)
        if(position EQUAL -1)
            message(FATAL_ERROR
                "Expected output substring not found: '${expected}'.\n"
                "Actual stdout:\n${normalized_stdout}"
            )
        endif()
    endif()
endforeach()
if(expectation_count EQUAL 0)
    message(FATAL_ERROR "At least one expected output line or substring is required.")
endif()

message(STATUS "CLI smoke contract passed (${expectation_count} output assertion(s)).")

if(NOT DEFINED CALCULATOR_EXECUTABLE OR CALCULATOR_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR "CALCULATOR_EXECUTABLE is required.")
endif()

execute_process(
    COMMAND "${CALCULATOR_EXECUTABLE}" divide 1 0
    RESULT_VARIABLE actual_exit
    OUTPUT_VARIABLE actual_stdout
    ERROR_VARIABLE actual_stderr
    TIMEOUT 5
)

if(NOT "${actual_exit}" STREQUAL "3")
    message(FATAL_ERROR
        "Expected division-by-zero exit code 3, received '${actual_exit}'.\n"
        "stdout: ${actual_stdout}\n"
        "stderr: ${actual_stderr}"
    )
endif()

if(NOT actual_stdout STREQUAL "")
    message(FATAL_ERROR "Expected no stdout, received: ${actual_stdout}")
endif()

string(REPLACE "\r\n" "\n" actual_stderr "${actual_stderr}")
if(NOT actual_stderr STREQUAL "error: division by zero\n")
    message(FATAL_ERROR
        "Unexpected division-by-zero diagnostic.\n"
        "Expected: error: division by zero\n"
        "Actual: ${actual_stderr}"
    )
endif()

message(STATUS "Calculator division-by-zero contract passed.")

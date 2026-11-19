if(NOT DEFINED CPPAPPLAB_TODO_EXECUTABLE OR CPPAPPLAB_TODO_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR "CPPAPPLAB_TODO_EXECUTABLE is required.")
endif()
if(NOT DEFINED CPPAPPLAB_TODO_TITLE_1 OR NOT DEFINED CPPAPPLAB_TODO_TITLE_2)
    message(FATAL_ERROR "Both CPPAPPLAB_TODO_TITLE_1 and CPPAPPLAB_TODO_TITLE_2 are required.")
endif()

set(work_directory "${CMAKE_CURRENT_BINARY_DIR}/cppapplab todo workflow")
file(REMOVE_RECURSE "${work_directory}")
file(MAKE_DIRECTORY "${work_directory}")
set(todo_file "${work_directory}/tasks.tsv")
set(todo_environment "CPPAPPLAB_TODO_FILE=${todo_file}")

function(run_cli_command expected_exit expected_stdout expected_stderr)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "${todo_environment}"
            "${CPPAPPLAB_TODO_EXECUTABLE}" ${ARGN}
        WORKING_DIRECTORY "${work_directory}"
        RESULT_VARIABLE actual_exit
        OUTPUT_VARIABLE actual_stdout
        ERROR_VARIABLE actual_stderr
        TIMEOUT 5
    )

    string(REPLACE "\r\n" "\n" actual_stdout "${actual_stdout}")
    string(REPLACE "\r\n" "\n" actual_stderr "${actual_stderr}")

    if(NOT "${actual_exit}" STREQUAL "${expected_exit}")
        message(FATAL_ERROR
            "CLI command '${ARGN}' returned '${actual_exit}', expected '${expected_exit}'.\n"
            "stdout: ${actual_stdout}\n"
            "stderr: ${actual_stderr}"
        )
    endif()

    if(NOT "${expected_stdout}" STREQUAL "__CPPAPPLAB_ANY_STDOUT__"
       AND NOT "${actual_stdout}" STREQUAL "${expected_stdout}")
        message(FATAL_ERROR
            "CLI command '${ARGN}' produced unexpected stdout.\n"
            "Expected: [${expected_stdout}]\n"
            "Actual:   [${actual_stdout}]"
        )
    endif()

    if(NOT "${actual_stderr}" STREQUAL "${expected_stderr}")
        message(FATAL_ERROR
            "CLI command '${ARGN}' produced unexpected stderr.\n"
            "Expected: [${expected_stderr}]\n"
            "Actual:   [${actual_stderr}]"
        )
    endif()

    set(CPPAPPLAB_LAST_TODO_STDOUT "${actual_stdout}" PARENT_SCOPE)
endfunction()

set(expected_after_add
    "#1 [ ] ${CPPAPPLAB_TODO_TITLE_1}\n#2 [ ] ${CPPAPPLAB_TODO_TITLE_2}\n"
)
set(expected_after_done
    "#1 [x] ${CPPAPPLAB_TODO_TITLE_1}\n#2 [ ] ${CPPAPPLAB_TODO_TITLE_2}\n"
)
set(expected_after_remove
    "#1 [x] ${CPPAPPLAB_TODO_TITLE_1}\n"
)

run_cli_command(0 "No tasks.\n" "" list)
run_cli_command(0 "__CPPAPPLAB_ANY_STDOUT__" "" add "${CPPAPPLAB_TODO_TITLE_1}")
run_cli_command(0 "__CPPAPPLAB_ANY_STDOUT__" "" add "${CPPAPPLAB_TODO_TITLE_2}")
run_cli_command(0 "${expected_after_add}" "" list)
run_cli_command(0 "__CPPAPPLAB_ANY_STDOUT__" "" done 1)
run_cli_command(0 "${expected_after_done}" "" list)
run_cli_command(0 "__CPPAPPLAB_ANY_STDOUT__" "" remove 2)
run_cli_command(0 "${expected_after_remove}" "" list)
run_cli_command(3 "" "error: task not found\n" done 999)

message(STATUS "Multi-process Todo CLI persistence workflow passed.")

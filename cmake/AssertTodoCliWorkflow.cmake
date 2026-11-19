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

function(run_cli_command expected_exit)
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

    set(CPPAPPLAB_LAST_TODO_STDOUT "${actual_stdout}" PARENT_SCOPE)
    set(CPPAPPLAB_LAST_TODO_STDERR "${actual_stderr}" PARENT_SCOPE)
endfunction()

function(assert_exact_output_line actual_output expected_line)
    set(padded_output "\n${actual_output}\n")
    string(FIND "${padded_output}" "\n${expected_line}\n" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "Expected exact output line not found: '${expected_line}'.\n"
            "Actual output:\n${actual_output}"
        )
    endif()
endfunction()

run_cli_command(0 list)
string(REGEX REPLACE "[\r\n]+$" "" initial_output "${CPPAPPLAB_LAST_TODO_STDOUT}")
if(NOT "${initial_output}" STREQUAL "No tasks.")
    message(FATAL_ERROR "Expected an empty Todo list, received: [${CPPAPPLAB_LAST_TODO_STDOUT}]")
endif()

run_cli_command(0 add "${CPPAPPLAB_TODO_TITLE_1}")
run_cli_command(0 add "${CPPAPPLAB_TODO_TITLE_2}")
run_cli_command(0 list)
assert_exact_output_line("${CPPAPPLAB_LAST_TODO_STDOUT}" "#1 [ ] ${CPPAPPLAB_TODO_TITLE_1}")
assert_exact_output_line("${CPPAPPLAB_LAST_TODO_STDOUT}" "#2 [ ] ${CPPAPPLAB_TODO_TITLE_2}")

run_cli_command(0 done 1)
run_cli_command(0 list)
assert_exact_output_line("${CPPAPPLAB_LAST_TODO_STDOUT}" "#1 [x] ${CPPAPPLAB_TODO_TITLE_1}")
assert_exact_output_line("${CPPAPPLAB_LAST_TODO_STDOUT}" "#2 [ ] ${CPPAPPLAB_TODO_TITLE_2}")

run_cli_command(0 remove 2)
run_cli_command(0 list)
assert_exact_output_line("${CPPAPPLAB_LAST_TODO_STDOUT}" "#1 [x] ${CPPAPPLAB_TODO_TITLE_1}")
string(FIND "${CPPAPPLAB_LAST_TODO_STDOUT}" "#2 " removed_task_position)
if(NOT removed_task_position EQUAL -1)
    message(FATAL_ERROR "Removed task is still present in the Todo list.")
endif()

run_cli_command(3 done 999)
assert_exact_output_line("${CPPAPPLAB_LAST_TODO_STDERR}" "error: task not found")

message(STATUS "Multi-process Todo CLI persistence workflow passed.")

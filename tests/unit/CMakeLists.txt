# Unit tests CMakeLists.txt
#
#  To add a new test:
#  1.  Write new test code: t_example.cpp.
#  2.  Add new to unit_tests.cpp in map() in main().
#  4.  Add declaration of new test to unit_tests.h.
#  5.  In this file:
#      (a) add source code to 'add_executable'
#      (b) add new 'add_test' for new test name.


# Create an executable target for our unit tests
add_executable(unit_tests unit_tests.cpp
                        t_read_rcf_file.cpp
                        t_read_progress_file.cpp
                        t_cputime_comparison.cpp
                        t_parse_key_value.cpp
                        t_parse_namelist_key_value.cpp
                        t_check_parse_int.cpp
                        t_fread_last_line.cpp
                        t_model_frac_done.cpp
                        t_path_exists.cpp
                        t_file_is_empty.cpp
                        t_zip_and_delete.cpp
                        t_set_env_var.cpp
                        t_get_out_files.cpp
                        t_launch_process.cpp
                        t_trickle_handler.cpp
                        t_run_process_with_timeout.cpp
                        t_verify_project_zip_md5.cpp
                        t_stage_model_input_archive.cpp
                        t_parse_control_input.cpp
)

# Helper executable used by launch_process tests
add_executable(launch_process_helper launch_process_helper.cpp)
add_executable(timed_process_helper timed_process_helper.cpp)

# Link the test executable to the control code
target_link_libraries(unit_tests PRIVATE cpdn_control)
target_compile_definitions(unit_tests PRIVATE LAUNCH_PROCESS_HELPER="$<TARGET_FILE:launch_process_helper>")
target_compile_definitions(unit_tests PRIVATE TIMED_PROCESS_HELPER="$<TARGET_FILE:timed_process_helper>")
add_dependencies(unit_tests launch_process_helper timed_process_helper)

# Add the targets to the CTest test suite
# The command is the path to the executable just built.
# CTest automatically runs this executable and checks its return code (0 = PASS, non-zero = FAIL).
add_test( NAME RCFTest       COMMAND unit_tests "Read RCF File" )
add_test( NAME ProgressTest  COMMAND unit_tests "Read Progress File" )
add_test( NAME CPUTimeComparisonTest COMMAND unit_tests "CPU Time Comparison" )
add_test( NAME ParseKeyValueTest COMMAND unit_tests "Parse Key Value" )
add_test( NAME ParseNamelistKeyValueTest COMMAND unit_tests "Parse Namelist Key Value" )
add_test( NAME ParseIntTest COMMAND unit_tests "Parse Int" )
add_test( NAME FreadLastLineTest COMMAND unit_tests "Fread Last Line" )
add_test( NAME ModelFracDoneTest COMMAND unit_tests "Model Frac Done" )
add_test( NAME PathExistsTest COMMAND unit_tests "Path Exists" )
add_test( NAME FileIsEmptyTest COMMAND unit_tests "File Is Empty" )
add_test( NAME ZipAndDeleteTest COMMAND unit_tests "Zip And Delete" )
add_test( NAME SetEnvVarTest COMMAND unit_tests "Set Env Var" )
add_test( NAME GetOutFilesTest COMMAND unit_tests "Get Out Files" )
add_test( NAME LaunchProcessTest COMMAND unit_tests "Launch Process" )
add_test( NAME TrickleHandlerTest COMMAND unit_tests "Trickle Handler" )
add_test( NAME RunProcessWithTimeoutTest COMMAND unit_tests "Run Process With Timeout" )
add_test( NAME VerifyProjectZipMD5Test COMMAND unit_tests "Verify Project Zip MD5" )
add_test( NAME StageModelInputArchiveTest COMMAND unit_tests "Stage Model Input Archive" )
add_test( NAME ParseControlInputTest COMMAND unit_tests "Parse Control Input" )

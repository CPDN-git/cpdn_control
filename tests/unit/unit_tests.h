#pragma once

#include <iostream>
#include <string>


// Handy macros to standardize part of the test code.
#define TEST( f ) std::string test_name( f )
#define SUCCESS std::cout << "TEST " << test_name << " succeeded.\n"
#define FAIL std::cout << "TEST " << test_name << " FAILED.\n"

// Declare all external test functions for main program (see individual test source files)
int t_read_rcf_file();
int t_read_progress_file();
int t_cputime_comparison();
int t_parse_key_value();
int t_parse_namelist_key_value();
int t_check_parse_int();
int t_fread_last_line();
int t_model_frac_done();
int t_path_exists();
int t_file_is_empty();
int t_zip_and_delete();
int t_set_env_var();
int t_get_out_files();
int t_launch_process();
int t_trickle_handler();
int t_run_process_with_timeout();
int t_verify_project_zip_md5();
int t_stage_model_input_archive();
int t_parse_control_input();
int t_control_start();
int t_logging_utils();

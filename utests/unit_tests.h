#pragma once

#include <string>
#include <iostream>


// Handy macros to standardize part of the test code.
#define TEST(f)  std::string test_name(f)
#define SUCCESS  std::cout << "TEST " << test_name << " succeeded.\n"
#define FAIL     std::cout << "TEST " << test_name << " FAILED.\n"

// Declare all external test functions for main program (see individual test source files)
int t_read_rcf_file();
int t_read_progress_file();
int t_cputime_comparison();
int t_parse_key_value();
int t_get_tag();
int t_check_stoi();
int t_fread_last_line();
int t_model_frac_done();
int t_path_exists();
int t_file_is_empty();

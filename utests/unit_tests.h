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

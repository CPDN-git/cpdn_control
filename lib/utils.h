//  Utility function declarations for the CPDN task controller
//       Glenn Carver, CPDN, 2025.

#pragma once

#include <string>
#include <vector>

bool set_env_var(const std::string&, const std::string&);
bool path_exists(std::string_view pathname);
bool file_is_empty(std::string_view fpath);
bool set_exec_perms(const std::string&);
bool parse_key_value(const std::string&, std::string&, std::string&);
bool extract_key_value(const std::string&, const std::string&, char, std::string& );
bool read_delimited_line(std::string, const std::string&, const std::string&, int, std::string&);
int  print_last_lines(const std::string& filename, const int nlines);
bool fread_last_line(const std::string&, std::string&);
std::string getDateTime();
std::vector<std::string> get_out_files(const std::string&);
void sleep_seconds(double seconds);
bool check_stoi(std::string& cin);
void banner(const std::string& model_name, const std::string& model_version, const std::string& code_version);
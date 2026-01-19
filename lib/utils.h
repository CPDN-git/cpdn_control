//  Utility function declarations for the CPDN task controller
//       Glenn Carver, CPDN, 2025.

#pragma once

#include <string>
#include <vector>

bool set_env_var( const std::string&, const std::string& );
bool path_exists( std::string_view pathname );
bool file_is_empty( std::string_view fpath );
bool set_exec_perms( const std::string& );
void trim_whitespace( std::string& );
bool parse_key_value( const std::string&, std::string&, std::string&, char );
bool parse_key_value( const std::string&, std::string&, std::string& );
bool parse_namelist_key_value( const std::string&, std::string&, std::string& );
int print_last_lines( const std::string& filename, const int nlines );
bool fread_last_line( const std::string&, std::string& );
std::string getDateTime();
std::vector<std::string> get_out_files( const std::string& );
void sleep_seconds( double seconds );
bool check_stoi( std::string& cin );
bool parse_int( std::string& value, int& out, std::string& err_msg );

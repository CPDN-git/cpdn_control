//  Header for OpeniFS model specific functions.

#pragma once

#include <fstream>
#include <string>
#include <vector>

std::string oifs_get_filename_part( const std::string&, const std::string& );
bool oifs_parse_stat( const std::string&, std::string&, const int );
bool oifs_valid_step( std::string&, int );
bool oifs_read_rcf_file( std::ifstream&, std::string&, std::string& );

//  Header for OpeniFS model specific functions.

#pragma once

#include <string>
#include <fstream>

bool oifs_setenvs(const std::string&, const std::string&);
std::string get_second_part(const std::string&, const std::string&);
bool oifs_parse_stat(const std::string&, std::string&, const int);
bool oifs_valid_step(std::string&,int);
bool oifs_read_rcf_file(std::ifstream&, std::string&, std::string&);
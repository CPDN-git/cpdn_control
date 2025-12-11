//
// Control code header file for the OpenIFS application in the climateprediction.net project
//
// Written by Andy Bowery (Oxford eResearch Centre, Oxford University) November 2022
// Contributions from Glenn Carver (ex-ECMWF), 2022->
//

#pragma once

#include <string>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <exception>
#include <algorithm>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <regex.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>   // for mkdir
#include <sys/resource.h>

#include "rapidxml.hpp"
#include "cpdn_zip.h"


int initialise_boinc(std::string&, std::string&, std::string&, int&);
int move_and_unzip_app_file(std::string, std::string, std::string, std::string);
int check_child_status(long, int);
int check_boinc_status(long, int);
long launch_process(const std::string&, const std::string&, const std::string&, const std::string&, const std::string&, const std::string&);
std::string get_tag(const std::string &str);
double model_frac_done(double, double, int);
int move_result_file(const std::string&, const std::string&, const std::string&);
void read_progress_file(std::string, int&, int&, std::string&, int&, int&);
void update_progress_file(std::string&, int, int, std::string&, int, int);
int copy_and_unzip(const std::string&, const std::string&, const std::string&, const std::string&);
bool process_env_overrides(const std::filesystem::path&);

using namespace rapidxml;

namespace chrono = std::chrono;
namespace     fs = std::filesystem;



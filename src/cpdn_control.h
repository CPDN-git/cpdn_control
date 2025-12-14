//
// Control code header file for the OpenIFS application in the climateprediction.net project
//
//     Glenn Carver, CPDN, 2025.
//      Original version by Andy Bowery, Oxford University November 2022
//

#pragma once

#include <string>
#include <filesystem>
#include <string>

#include "cpdn_main.h"


int initialise_boinc(std::string&, std::string&, std::string&, int&);
int move_and_unzip_app_file(std::string, std::string, std::string, std::string);
int check_child_status(long, int);
int check_boinc_status(long, int);
long launch_process(const std::string&, const std::string&, const std::string&, const std::string&, const std::string&, const std::string&);
std::string get_tag(const std::string &str);
double model_frac_done(double, double, int);
int move_result_file(const std::string&, const std::string&, const std::string&);
void read_progress_file(std::string_view, TaskState&);
void update_progress_file(std::string_view, const TaskState&);
int copy_and_unzip(const std::string&, const std::string&, const std::string&, const std::string&);
bool process_env_overrides(const std::filesystem::path&);

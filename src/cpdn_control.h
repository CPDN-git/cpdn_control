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


/**
 * @struct TaskState
 * @brief Encapsulates all task-related state variables for managing model execution.
 *        Groups logically related variables for better code organization and clarity.
 */
struct TaskState {
    int last_cpu_time = 0;          // CPU time at last checkpoint
    int upload_file_number = 0;     // Sequential counter for upload files
    std::string last_iter = "0";    // Last completed iteration step
    int last_upload = 0;            // Time of last upload file (in seconds)
    int model_completed = 0;        // Model completion state: 0=running, 1=completed
    int current_iter = 0;           // Current iteration step (in seconds)
    int last_trickle_iter = 0;      // Last iteration when trickle was sent
    int process_status = 1;         // Child process status: 0=running, 1=stopped, etc.
    double current_cpu_time = 0.0;  // Current accumulated CPU time
    double fraction_done = 0.0;     // Fraction of model run completed (0.0-1.0)
};


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

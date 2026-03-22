//
// Control code header file for the OpenIFS application in the climateprediction.net project
//
//     Glenn Carver, CPDN, 2025.
//      Rewrite of original version by Andy Bowery, Oxford University November 2022
//

#pragma once

#include <filesystem>
#include <string>
#include <vector>


// GC. TODO. Consider splitting these sturcts into separate header files.

/**
 * @struct TaskState
 * @brief Encapsulates all task-related state variables for managing model execution.
 *        Groups logically related variables for better code organization and clarity.
 */
struct TaskState {
    double prior_acc_cpu_time = 0.0;    // Accumulated CPU time saved from earlier model runs before current child started
    int upload_file_number = 0;         // Sequential counter for upload files
    std::string last_step = "0";        // Last completed model step
    int last_upload = 0;                // Time of last upload file (in seconds)
    int model_completed = 0;            // Model completion state: 0=started/running, 1=completed; does NOT imply it worked!
    bool model_success = false;         // Model run success flag: false=failed, true=successful
    int current_step = 0;               // Current model step (in seconds) ? really secs?
    int last_trickle_step = 0;          // Last model step when trickle was sent
    pid_t pid = 0;                      // Process ID of the child model process
    int process_status = 1;             // Child process status: 0=running, 1=stopped, etc.
    int exit_code = 0;                  // Child process exit code (valid for normal exit)
    double current_cpu_time = 0.0;      // Current accumulated CPU time
    double fraction_done = 0.0;         // Fraction of model run completed (0.0-1.0)
};


/**
 * @struct TaskConfig
 * @brief Encapsulates all CPDN specific task-related configuration parameters.
 */
struct TaskConfig {
    std::string batch;        // Batch ID
    std::string workunit;     // Workunit ID
    std::string memberid;     // Unique member ID (umid)
    std::string startdate;    // Simulation start date
    std::string exptid;       // Experiment ID for the model run << GC. Why is this here? Should be in model obj.
    std::string fclen;        // Forecast length in days : need this before model starts for filenames. It should match with the model!
};


/**
 * @struct BoincConfig
 * @brief Encapsulates BOINC configuration parameters for the task.
 *        Variables extracted from BOINC APP_INIT_DATA structure after boinc initialization.
 *        See boinc code lib/app_ipc.h for more details.
 */
struct BoincConfig {
    std::string app_version;               // Application version
    std::string app_name;                  // Application name
    std::string project_dir;               // Project directory path
    std::string wu_name;                   // Workunit name
    std::string result_name;               // Result name
    int ncpus;                             // Multicore apps only. CPDN only use CPU based tasks.
    std::string boinc_dir;                 // BOINC data/main directory (not currently used).
    std::vector<std::string> app_files;    // List of files in the app version (not currently used)

    std::string slot_path;      // Slot directory path; APP_INIT_DATA only gives slot number.
    bool standalone = false;    // Standalone mode flag
};


int init_boinc( BoincConfig& );
int move_and_unzip_app_file( const std::string&, const std::string&, const std::string&, const std::string& );
int check_child_status( pid_t, int, int& );
int check_boinc_status( pid_t, int );
pid_t launch_process( const std::string&, const std::string&, const std::string&, const std::string& );
std::string get_tag( const std::string& str );
double model_frac_done( double, double, int );
int move_result_file( const std::string&, const std::string&, const std::string& );
int copy_and_unzip( const std::string&, const std::string&, const std::string&, const std::string& );
bool process_env_overrides( const std::filesystem::path& );
int zip_and_delete( const std::string&, const std::vector<std::filesystem::path>& );

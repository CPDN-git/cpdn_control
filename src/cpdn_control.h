//
// Control code header file for the OpenIFS application in the climateprediction.net project
//
//     Glenn Carver, CPDN, 2025.
//      Rewrite of original version by Andy Bowery, Oxford University November 2022
//

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "boinc/boinc_api.h"    // for BOINC_STATUS and BOINC_OPTIONS structs and boinc API function declarations

#include "api/model_input_manifest.h"
#include "lib/process_control.h"

class ModelControl;

// GC. TODO. Consider splitting these structs into separate header files.

/**
 * @struct TaskState
 * @brief Encapsulates all task-related state variables for managing model execution.
 *        Groups logically related variables for better code organization and clarity.
 */
struct TaskState {
    double prior_acc_cpu_time = 0.0;     // Accumulated CPU time saved from earlier model runs before current child started
    int upload_file_number = 0;          // Sequential counter for upload files
    int last_completed_step = 0;         // Last completed model step count
    double last_upload_time = 0.0;       // Elapsed model time at the last upload, in seconds
    int model_completed = 0;             // Model completion state: 0=started/running, 1=completed; does NOT imply it worked!
    bool model_success = false;          // Model run success flag: false=failed, true=successful
    int current_step = 0;                // Current model step count
    int last_trickle_step = 0;           // Last model step count for which a trickle was sent
    ChildProcessHandle child_process;    // Child process handle and portable process id for the model child.
    int child_status = 1;             // Child process status: 0=running, 1=exited normally, 3=abnormal/forced termination, 4=suspended, 5=not found.
    int exit_code = 0;                // Child process exit code (valid for normal exit)
    double current_cpu_time = 0.0;    // Current process accum CPU time; add prior_acc_cpu_time for total task CPU time.
    double restart_cpu_time = 0.0;    // Current process accum CPU time at latest restart; add prior_acc_cpu_time for restart total task CPU time.
    double fraction_done = 0.0;       // Fraction of model run completed (0.0-1.0)
};


/**
 * @struct TaskConfig
 * @brief Encapsulates all CPDN specific task-related configuration parameters passed on the command line.
 */
struct TaskConfig {
    std::string batch;                 // Batch ID
    std::string workunit;              // Workunit ID
    std::string memberid;              // Unique member ID (umid)
    std::string filename_startdate;    // CPDN filename token used to resolve task download filenames; not passed to the model.
    std::string filename_fclen;        // CPDN filename token used to resolve task download filenames; not passed to the model.
    int upload_interval = 0;           // Upload interval in model steps; 0 disables result uploads but not trickles.
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

/**
 * @struct BoincRuntime
 * @brief Holds the latest live BOINC runtime status snapshot.
 */
struct BoincRuntime {
    BOINC_STATUS client_status{};
};

/**
 * @struct InputStageResult
 * @brief Structured result for BOINC/model input staging so main() can report the
 *        exact logical file, resolved project file and failure step when staging
 *        fails on a remote host.
 */
struct InputStageResult {
    bool ok = false;
    std::filesystem::path logical_file;
    std::filesystem::path resolved_project_file;
    std::filesystem::path destination_archive;
    std::string step;
    std::string message;
};


int init_boinc( BoincConfig& );
int stage_and_unzip_app_file( const std::string&, const std::string&, const std::string&, const std::string& );
int check_child_status( ChildProcessHandle&, int, int& );
bool handle_boinc_client_status( ChildProcessHandle&, BoincRuntime& );
ChildProcessHandle launch_process( const ModelControl&, const std::string&, const std::string&, const std::string&, const std::string& );
double model_frac_done( double, double, int );
int move_result_file( const std::filesystem::path&, const std::filesystem::path&, const std::string& );
int zip_and_delete( const std::string&, const std::vector<std::filesystem::path>& );
bool resolve_boinc_input_file( const std::filesystem::path&, std::filesystem::path&, std::string* error_msg = nullptr );
bool verify_project_zip_md5( const std::filesystem::path&, std::string* error_msg = nullptr );
InputStageResult stage_model_input_archive( const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&,
                                            std::string_view );
InputStageResult stage_boinc_input_file( const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&, std::string_view );
InputStageResult stage_model_input_manifest( const ModelInputManifest&, const std::filesystem::path& );

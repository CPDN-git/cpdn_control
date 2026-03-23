//  Utility function declarations for the CPDN task controller
//       Glenn Carver, CPDN, 2025.

#pragma once

#include <filesystem>
#include <utility>
#include <string>
#include <vector>

/**
 * @brief Status values returned by run_process_with_timeout().
 *        Distinguishes launch, wait, timeout, and child-exit outcomes.
 */
enum class TimedProcessStatus {
    success,
    spawn_failed,
    wait_failed,
    timed_out,
    child_failed,
};

/**
 * @brief Result from a timed external process run.
 *        Includes the process outcome, exit code, and whether the expected output file was refreshed.
 */
struct TimedProcessResult {
    TimedProcessStatus status = TimedProcessStatus::spawn_failed;
    int exit_code = -1;
    bool output_updated = false;
};

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
bool parse_int( std::string& value, int& out, std::string& err_msg );
bool parse_int( std::string& value );

/**
 * @brief Run an executable in a given working directory and wait up to a timeout.
 *        Accepts optional argv-style arguments, can set child environment variables before exec,
 *        can check whether an expected output file was created or updated,
 *        and can optionally redirect child stdout/stderr to a combined log file.
 */
TimedProcessResult run_process_with_timeout(
    const std::string& executable,
    const std::vector<std::string>& args,
    const std::string& working_dir,
    int timeout_seconds,
    const std::filesystem::path& expected_output_file = {},
    const std::vector<std::pair<std::string, std::string>>& child_env_vars = {},
    const std::filesystem::path& combined_output_file = {} );

/**
 * @brief Convert a TimedProcessStatus value to a short log-friendly string.
 *        Used for controller diagnostics and error reporting.
 */
const char* timed_process_status_to_string( TimedProcessStatus status );

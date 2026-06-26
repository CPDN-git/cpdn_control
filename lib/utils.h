//  Utility function declarations for the CPDN task controller
//       Glenn Carver, CPDN, 2025.

#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <utility>
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

/**
 * @brief Rolling window of recent observed model step deltas used to tune main-loop polling.
 */
struct StepDeltaAverageWindow {
    static constexpr std::size_t window_size = 5;

    std::array<int, window_size> values{};
    std::size_t count = 0;
    std::size_t next_index = 0;
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
bool ensure_directory( const std::filesystem::path&, std::string* error_msg = nullptr );
bool is_ascii_alpha( std::string_view text );
bool is_ascii_digit( std::string_view text );
bool is_ascii_digit( char ch );

/**
 * @brief Run an executable in a given working directory and wait up to a timeout.
 *        Accepts optional argv-style arguments, can set child environment variables before exec,
 *        can check whether an expected output file was created or updated,
 *        and can optionally redirect child stdout/stderr to a combined log file.
 */
TimedProcessResult run_process_with_timeout( const std::string& executable, const std::vector<std::string>& args, const std::string& working_dir,
                                             int timeout_seconds, const std::filesystem::path& expected_output_file = {},
                                             const std::vector<std::string>& child_env_vars = {},
                                             const std::filesystem::path& combined_output_file = {} );

/**
 * @brief Convert a TimedProcessStatus value to a short log-friendly string.
 *        Used for controller diagnostics and error reporting.
 */
const char* timed_process_status_to_string( TimedProcessStatus status );

/**
 * @brief Add a positive observed step delta to the rolling window.
 */
void record_step_delta( StepDeltaAverageWindow& window, int step_delta );

/**
 * @brief Return the average of the recorded positive step deltas, or 0.0 when empty.
 */
double average_step_delta( const StepDeltaAverageWindow& window );

/**
 * @brief Report whether the current observed step delta exceeds the existing rolling average.
 */
bool step_delta_exceeds_average( const StepDeltaAverageWindow& window, int step_delta );

/**
 * @brief Decrease a loop delay by a fixed amount without dropping below a minimum.
 */
double reduce_loop_delay_seconds( double current_delay_seconds, double decrement_seconds, double minimum_delay_seconds );

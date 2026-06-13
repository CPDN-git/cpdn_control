#include "external_diagnostics.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include "api/model_control.h"
#include "api/trickle_handler.h"
#include "lib/utils.h"

namespace fs = std::filesystem;

namespace {

constexpr int DIAGNOSTICS_TIMEOUT_SECONDS = 30;
constexpr std::string_view DIAGNOSTICS_INPUT_PREFIX = "ICMSH";    // experimental

/**
 * @brief Return the ICMSH filename to use as diagnostics input for the completed step.
 *        Returns an empty string when no matching diagnostics input file is present in the slot directory.
 */
std::string get_diagnostics_input_file( const fs::path& slot_path, const std::vector<std::string>& output_files )
{
    for ( const auto& output_file : output_files ) {
        if ( output_file.rfind( DIAGNOSTICS_INPUT_PREFIX.data(), 0 ) != 0 ) {
            continue;
        }

        fs::path output_path = slot_path / output_file;
        if ( fs::exists( output_path ) ) {
            return output_file;
        }
    }

    return "";
}

/**
 * @brief Replay the diagnostics program output log to stderr and remove the log file.
 */
void replay_diagnostics_output_to_stderr( const fs::path& combined_output_path )
{
    if ( combined_output_path.empty() || !fs::exists( combined_output_path ) ) {
        return;
    }

    std::ifstream diagnostics_log( combined_output_path );
    if ( !diagnostics_log ) {
        std::cerr << "Warning: failed to open diagnostics output log: " << combined_output_path << '\n';
    } else {
        std::cerr << "Diagnostics stdout/stderr follows from " << combined_output_path.filename().string() << ":\n";
        std::cerr << diagnostics_log.rdbuf();
        if ( !std::cerr.good() ) {
            std::cerr.clear();
            std::cerr << "Warning: failed while replaying diagnostics output to controller stderr\n";
        }
    }

    std::error_code ec;
    fs::remove( combined_output_path, ec );
    if ( ec ) {
        std::cerr << "Warning: failed to remove diagnostics output log: " << combined_output_path << ": " << ec.message() << '\n';
    }
}

}    // namespace

bool run_step_diagnostics( const ModelControl& model_ctrl, const fs::path& diag_exe, const fs::path& slot_path,
                           const std::vector<std::string>& output_files )
{
    std::string diagnostics_input = get_diagnostics_input_file( slot_path, output_files );
    if ( diag_exe.empty() || diagnostics_input.empty() ) {
        return false;
    }

    // GC ** EXPERIMENTAL **
    // Currently 'diagnostics.exe' is 'sptogp_parest.exe', which is a modified version
    // of sptogp that read the ICMSH output file, transforms to gridpoint and computes
    // zonal mean of zonal wind over the Pacific region defined by Chris O'Reilly.
    // If successful this should be handled by the oifs class.

    // Args to sptogp are:
    // -s <input file> -G <output file>
    // -t f :  transform to full grid, not reduced grid
    // -l   :  linear grid
    // -f 131 : grid field code to convert; 131 is U wind.
    // -n   : do not truncate output field.
    // -p ./rtables/ : path for resolution tables.

    fs::path trickle_data_path = slot_path / TrickleHandler::TRICKLE_DATA_FILE;
    fs::path diagnostics_log_path = slot_path / "diagnostics_output.log";
    std::string err_msg;
    std::vector<std::string> diag_env_vars = model_ctrl.get_env_vars( slot_path.string(), "1", err_msg );
    if ( !err_msg.empty() ) {
        std::cerr << "Warning: failed to prepare diagnostics environment: " << err_msg << '\n';
        return false;
    }
    std::vector<std::string> diag_args = { "-s", diagnostics_input, "-G", diagnostics_input + ".diag", "-t", "f", "-l", "-f", "131", "-n",
                                           "-p", "./rtables/" };
    std::error_code ec;
    fs::remove( trickle_data_path, ec );
    if ( ec ) {
        std::cerr << "Warning: failed to remove stale trickle_data before diagnostics run: " << ec.message() << '\n';
        ec.clear();
    }
    fs::remove( diagnostics_log_path, ec );
    if ( ec ) {
        std::cerr << "Warning: failed to remove stale diagnostics output log before diagnostics run: " << ec.message() << '\n';
        ec.clear();
    }

    std::cerr << "Running external diagnostics program: " << diag_exe << " with input file: " << diagnostics_input << '\n';
    TimedProcessResult diag_result = run_process_with_timeout( diag_exe.string(), diag_args, slot_path.string(), DIAGNOSTICS_TIMEOUT_SECONDS,
                                                               trickle_data_path, diag_env_vars, diagnostics_log_path );
    replay_diagnostics_output_to_stderr( diagnostics_log_path );

    if ( diag_result.status != TimedProcessStatus::success ) {
        std::cerr << "Warning: diagnostics program failed with status " << timed_process_status_to_string( diag_result.status );
        if ( diag_result.exit_code >= 0 ) {
            std::cerr << ", exit_code=" << diag_result.exit_code;
        }
        std::cerr << '\n';
        fs::remove( trickle_data_path, ec );
        return true;
    }

    if ( !diag_result.output_updated ) {
        std::cerr << "Warning: diagnostics program completed but did not produce a fresh trickle_data file\n";
        fs::remove( trickle_data_path, ec );
        return true;
    }

    std::cerr << "Diagnostics completed and updated trickle_data\n";
    return true;
}

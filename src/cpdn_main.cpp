//
// BOINC task controller for CPDN.
//
// This version written by Glenn Carver, CPDN, 2025->
// Complete rewrite of original version by Andy Bowery (OERC) December 2023.
//

#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <thread>
#include <vector>

#include "boinc/boinc_api.h"
#include "boinc/error_numbers.h"

#include "control_start.h"
#include "cpdn_control.h"
#include "cpdn_zip.h"
#include "external_diagnostics.h"
#include "lib/cpdn_cpu_time.h"
#include "lib/logging_utils.h"
#include "lib/utils.h"
#include "parse_args.h"

#include "api/model_control.h"
#include "api/progressfile_handler.h"
#include "api/trickle_handler.h"

#include "models/openifs/oifs_control.h"

namespace chrono = std::chrono;
namespace fs = std::filesystem;


// Define the code version if not defined at compile time with -D option.
#ifndef CODE_VERSION
#define CODE_VERSION "1.0.0"
#endif


// Constants
constexpr std::string_view MODEL_CONFIG_FILE = "model_config.xml";
constexpr int LOOP_DELAY_DEFAULT = 7;
constexpr int LOOP_DELAY_FAST = 1;


// Data structures for capturing rich context for error reporting.
struct UploadSendResult {
    bool ok = true;
    bool archive_created = false;
    bool upload_attempted = false;
    fs::path archive_path;
    std::string logical_upload_name;
    std::string error_step;
    int error_code = 0;
    int finish_code = 1;
    std::string error_message;
};


// ------------------------------------------
// --------------- Functions ----------------

/**
 * @brief Factory function to create ModelControl instance based on model name.
 *        Note that we specify the *model* name here and not the app name though 
 *        they may be the same as the app_name is typically passed to the controller.
 * 
 * @param modelName The name of the model.
 * @return A unique pointer to the created ModelControl instance. Maybe nullptr if model not supported.
*/
static std::unique_ptr<ModelControl> create_model_control( std::string_view model_name, std::string_view model_version )
{
    std::unique_ptr<ModelControl> model;    // create a null unique_ptr ready for a new model control instance.

    // Model mappings
    // As the test model is an OpenIFS skeleton clone, we use the OpenIFSControl class.

    if ( model_name == "test_model" ) {
        model = std::make_unique<OpenIFSControl>( "CPDN", model_name, model_version, "test_model" );

    } else if ( model_name == "oifs_43r3_omp_l159" || model_name == "oifs_43r3_omp_l319" || model_name == "oifs_43r3_parest_omp_l319" ) {
        model = std::make_unique<OpenIFSControl>( "ECMWF", model_name, model_version, "oifs_43r3_omp_model.exe" );
    }

    return model;
}


/**
 * @brief Report failure to stage an input file with as much context as possible.
 */
static void report_input_stage_failure( std::string_view context, const InputStageResult& result )
{
    std::cerr << "..Failed to stage " << context;
    if ( !result.logical_file.empty() ) {
        std::cerr << " for logical file '" << result.logical_file << "'";
    }
    if ( !result.resolved_project_file.empty() ) {
        std::cerr << " resolved to '" << result.resolved_project_file << "'";
    }
    if ( !result.destination_archive.empty() ) {
        std::cerr << " via slot archive '" << result.destination_archive << "'";
    }
    if ( !result.step.empty() ) {
        std::cerr << " at step '" << result.step << "'";
    }
    if ( !result.message.empty() ) {
        std::cerr << ": " << result.message;
    }
    std::cerr << '\n';
}


/**
 * @brief Report failure to parse the model control input with as much context as possible.
 */
static void report_model_control_input_failure( const ModelControlInputData& result )
{
    std::cerr << "..Failed to parse model control input";
    if ( !result.source_file.empty() ) {
        std::cerr << " '" << result.source_file.string() << "'";
    }
    if ( !result.error_step.empty() ) {
        std::cerr << " at step '" << result.error_step << "'";
    }
    if ( !result.error_field.empty() ) {
        std::cerr << " for field '" << result.error_field << "'";
    }
    if ( !result.error_message.empty() ) {
        std::cerr << ": " << result.error_message;
    }
    std::cerr << '\n';
}


/**
 * @brief Refresh the accumulated CPU time for the running model process.
 *        Leaves the previous value intact if the process CPU time cannot be read.
 */
static void refresh_current_cpu_time( TaskState& tstate )
{
    double child_cpu_time = cpdn_cpu_time( tstate.child_process.process_id );
    if ( child_cpu_time > 0.0 ) {
        tstate.current_cpu_time = tstate.prior_acc_cpu_time + child_cpu_time;
    }
}


/**
 * @brief Parse a string as a double and validate that entire string was consumed and the value is in range.
 */
static bool parse_double_arg( const std::string& text, double& value, std::string& err_msg )
{
    char* end = nullptr;
    errno = 0;
    value = std::strtod( text.c_str(), &end );

    if ( end == text.c_str() ) {
        err_msg = "invalid floating-point value";
        return false;
    }
    if ( errno == ERANGE ) {
        err_msg = "floating-point value out of range";
        return false;
    }
    if ( end == nullptr || *end != '\0' ) {
        err_msg = "unexpected trailing characters in floating-point value";
        return false;
    }
    return true;
}


/**
 * @brief Convert a model step count to elapsed model time in seconds based on the timestep interval.
 */
static double step_to_model_time( int step, int timestep_seconds ) { return static_cast<double>( step ) * static_cast<double>( timestep_seconds ); }


/**
 * @brief Report failure to zip and send an upload archive with as much context as possible.
 */
static void report_upload_send_failure( std::string_view context, const UploadSendResult& result )
{
    std::cerr << "..Failed to send " << context;
    if ( !result.archive_path.empty() ) {
        std::cerr << " archive '" << result.archive_path << "'";
    }
    if ( !result.logical_upload_name.empty() ) {
        std::cerr << " as logical file '" << result.logical_upload_name << "'";
    }
    if ( !result.error_step.empty() ) {
        std::cerr << " at step '" << result.error_step << "'";
    }
    if ( result.error_code != 0 ) {
        std::cerr << " (code " << result.error_code << ")";
    }
    if ( !result.error_message.empty() ) {
        std::cerr << ": " << result.error_message;
    }
    std::cerr << '\n';
}


/**
 * @brief Determine the appropriate exit code for the task based on the child process status and BOINC runtime status.
 *       Returns 0 for normal completion or if a quit request was made, 
 *       and 1 for abort/no heartbeat or if the child process did not exit normally.
 */
static int get_task_finish_code( const TaskState& tstate, const BoincRuntime& bruntime )
{
    if ( bruntime.client_status.quit_request ) {
        return 0;
    }
    if ( bruntime.client_status.abort_request || bruntime.client_status.no_heartbeat ) {
        return 1;
    }
    if ( tstate.child_status == 1 ) {
        return 0;
    }
    return 1;
}


/**
 * @brief Sleep in short chunks and poll BOINC state between chunks.
 *        Returns false if BOINC status changed and the caller should handle it.
 *
 * @note This function only polls BOINC. It does not stop, resume, or kill the child.
 */
static bool sleep_with_boinc_poll( BoincRuntime& bruntime, const bool standalone, const int total_seconds )
{
    if ( total_seconds <= 0 ) {
        return true;
    }

    auto deadline = chrono::steady_clock::now() + chrono::seconds( total_seconds );
    constexpr auto poll_interval = chrono::seconds( 5 );

    while ( chrono::steady_clock::now() < deadline ) {
        auto remaining = chrono::duration_cast<chrono::seconds>( deadline - chrono::steady_clock::now() );
        auto sleep_chunk = std::min( poll_interval, remaining );
        sleep_seconds( static_cast<double>( sleep_chunk.count() ) );

        if ( standalone ) {
            continue;
        }

        boinc_get_status( &bruntime.client_status );
        if ( bruntime.client_status.suspended || bruntime.client_status.quit_request || bruntime.client_status.abort_request ||
             bruntime.client_status.no_heartbeat ) {
            return false;
        }
    }

    return true;
}


/**
 * @brief Zip a prepared upload file set and, when running under BOINC, submit the logical upload file.
 */
static UploadSendResult zip_and_send_upload( const BoincConfig& bconfig, BoincRuntime& bruntime, TaskState& tstate,
                                             const std::string& result_base_name, const int upload_file_number,
                                             const std::vector<fs::path>& files_to_zip )
{
    UploadSendResult result;
    result.archive_path = fs::path( bconfig.project_dir ) / ( result_base_name + "_" + std::to_string( upload_file_number ) + ".zip" );
    result.logical_upload_name = "upload_file_" + std::to_string( upload_file_number ) + ".zip";

    if ( files_to_zip.empty() ) {
        return result;
    }

    std::cerr << "Compressing upload file: " << result.archive_path << '\n';
    int zip_ret = zip_and_delete( result.archive_path.string(), files_to_zip );
    if ( zip_ret != 0 ) {
        result.ok = false;
        result.error_step = "zip";
        result.error_code = zip_ret;
        result.error_message = "failed to create upload archive";
        return result;
    }
    result.archive_created = true;

    if ( bconfig.standalone ) {
        return result;
    }

    result.upload_attempted = true;

    std::cerr << "Waiting for file operations to complete...(20 secs)" << std::endl;
    if ( !sleep_with_boinc_poll( bruntime, bconfig.standalone, 20 ) ) {
        if ( !handle_boinc_client_status( tstate.child_process, bruntime ) ) {
            result.ok = false;
            result.error_step = "boinc_poll";
            result.error_message = "BOINC status changed before upload could be submitted";
            result.finish_code = get_task_finish_code( tstate, bruntime );
            return result;
        }
    }

    std::string upload_name = result.logical_upload_name;
    int upload_ret = boinc_upload_file( upload_name );
    if ( upload_ret != 0 ) {
        result.ok = false;
        result.error_step = "boinc_upload_file";
        result.error_code = upload_ret;
        result.error_message = boincerror( upload_ret );
        return result;
    }

    int upload_status_ret = boinc_upload_status( upload_name );
    if ( upload_status_ret != 0 ) {
        result.ok = false;
        result.error_step = "boinc_upload_status";
        result.error_code = upload_status_ret;
        result.error_message = boincerror( upload_status_ret );
        return result;
    }

    return result;
}


/**
 * @brief Parse and validate an optional trailing --nthreads argument from app_config.xml.
 *
 * @param argc Program argument count.
 * @param argv Program argument vector.
 * @param nthreads In/out thread count. Caller seeds the default from BOINC init_data.xml.
 * @param used_app_config_nthreads True when a valid app_config.xml override was applied.
 * @param err_msg Error string if parsing fails.
 * @returns True when the optional override was handled successfully, false on parse failure.
 */
static bool get_app_config_nthreads( int argc, char** argv, int& nthreads, bool& used_app_config_nthreads, std::string& err_msg )
{
    err_msg.clear();
    used_app_config_nthreads = false;

    if ( argc < 2 ) {
        return true;
    }

    if ( std::string( argv[argc - 1] ) == "--nthreads" ) {
        std::cerr << "Warning. --nthreads argument present but has no value! Ignoring.\n";
        return true;
    }

    if ( argc < 3 || std::string( argv[argc - 2] ) != "--nthreads" ) {
        return true;
    }

    // GC. The best max as parallel efficiency markedly drops after this many threads, even at T319.
    int max_threads = 8;
    int min_threads = 1;    // minimum number of threads.
    int requested_nthreads = -1;

    std::string nthreads_value = argv[argc - 1];
    if ( !parse_int( nthreads_value, requested_nthreads, err_msg ) ) {
        std::cerr << "Warning. --nthreads argument must be a valid integer! " << err_msg << '\n';
        return false;
    }

    used_app_config_nthreads = true;
    nthreads = requested_nthreads;

    if ( requested_nthreads > max_threads ) {
        std::cerr << "Warning. --nthreads value too high. Setting to max number of threads : " << max_threads << '\n';
        nthreads = max_threads;
    } else if ( requested_nthreads < min_threads ) {
        std::cerr << "Warning. --nthreads is too low for this configuration. Minimum #threads is 1. Resetting.\n";
        nthreads = min_threads;
    }

    return true;
}


/**
 * @brief Construct the result base name for result files.
 *        When running under BOINC, this comes from the resolved part
 *        of the first upload file. When running standalone, we make
 *        up a reasonable name based on the workunit parameters.
 * @return Result base name string (without path or .zip)
 */
static std::string get_result_base_name( const BoincConfig& bconfig, const TaskConfig& tconfig )
{
    std::string base_name;

    if ( !bconfig.standalone ) {
        std::string resolved_name;
        int retval = boinc_resolve_filename_s( "upload_file_0.zip", resolved_name );
        if ( retval ) {
            std::cerr << "..boinc_resolve_filename failed" << std::endl;
            return base_name;
        }

        base_name = fs::path( resolved_name ).stem().string();    // returns filename without path nor '.zip'
        if ( base_name.length() > 2 ) {
            base_name.erase( base_name.length() - 2 );    // remove the '_0'
        }
        if ( base_name.compare( "upload_file" ) == 0 ) {
            std::cerr << "..Failed to get result name" << std::endl;
            return base_name;
        }
    } else {
        base_name = bconfig.app_name + "_" + tconfig.filename_startdate + "_" + tconfig.batch + "_" + tconfig.workunit;
    }
    return base_name;
}

/**
 * @brief Append upload files that match the expected output filename pattern.
 *
 * @returns zero on success, otherwise error code value.
 */
static int add_upload_files( const fs::path& dir, std::vector<fs::path>& out, const std::regex& pattern )
{
    std::error_code ec;

    for ( const auto& entry : fs::directory_iterator( dir, ec ) ) {
        if ( ec ) {
            std::cerr << "..Unable to scan upload directory: " << dir << " (" << ec.message() << ")\n";
            return ec.value();
        }
        if ( !entry.is_regular_file( ec ) ) {
            if ( ec ) {
                std::cerr << "..Unable to read directory entry: " << entry.path() << " (" << ec.message() << ")\n";
                return ec.value();
            }
            continue;
        }

        const auto filename = entry.path().filename().string();
        if ( std::regex_match( filename, pattern ) ) {
            out.push_back( entry.path() );
            std::cerr << "Adding to the zip: " << entry.path().string() << '\n';
        }
    }
    return 0;
}

/**
 * @brief Prints a banner to stderr at start of controller with model name and version.
 */
static void banner( const BoincConfig& bc, const std::string& code_version )
{
    std::cerr << "\n\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    std::cerr << "|  CPDN task controller starting: version " << code_version << " \n";
    std::cerr << "|  App name: " << bc.app_name << ". App version: " << bc.app_version << " \n";
    std::cerr << "|  Workunit name: " << bc.wu_name << " \n";
    std::cerr << "|  Slot path:     " << bc.slot_path << " \n";
    std::cerr << "|  Project directory: " << bc.project_dir << " \n";
    std::cerr << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n\n";
}


/**
 * @brief Cleanup and finish the task.
 *        Closes any child-process handle state, ends any BOINC critical section,
 *        then calls boinc_finish() and returns the same exit code.
 *        boinc_finish exits under BOINC, but return is kept for dummy libraries.
 */
static int finish_task( TaskState& tstate, int exit_code )
{
    close_child_process_handle( tstate.child_process );
    boinc_end_critical_section();    // in case we abort while in critical section (boinc api handles case if not in critical section).
    boinc_finish( exit_code );       // boinc_finish exits, no further code executed after this call (unless a dummy library is used).
    return exit_code;
}


//----------------------------------------------------------------------------
//----------------------------- MAIN PROGRAM ---------------------------------
//----------------------------------------------------------------------------


int main( int argc, char** argv )
{
    BoincConfig bconfig;    // BOINC settings from init_data.xml.
    TaskConfig tconfig;     // CPDN task settings from command line.
    TaskState tstate;       // Task state is declared early so finish paths can always release child resources.
    int retval = 0;
    std::string err_msg;

    // ------------- BOINC Initialisation -----------------

    // Initialise BOINC to get the project directory, workunit name and app version
    // Note this redirects stderr output to stderr.txt in slot dir.
    retval = init_boinc( bconfig );
    if ( retval ) {
        std::cerr << "..BOINC initialisation failed" << "\n";
        return finish_task( tstate, retval );
    }

    // Install a temporary streambuf wrapper on std::cerr so each new log line gets
    // a date/time prefix automatically. This keeps the existing stream-style logging
    // code intact while making remote stderr logs easier to correlate and debug.
    Timestamped timestamped_cerr( std::cerr );

    if ( bconfig.slot_path.empty() ) {
        std::cerr << "..Error. Can't determine slot path: current_path() returned empty" << std::endl;
        return finish_task( tstate, 1 );
    }

    // Say who we are.
    banner( bconfig, CODE_VERSION );
    if ( bconfig.standalone ) {
        std::cerr << "Running in standalone mode" << '\n';
    }

    // ---------------- Task configuration -----------------

    // Read in the model config.xml.  The XML file contains all the information
    // about the model. It's required to initialize the correct model class later on.
    // GlennC. May not need this in the future. to be determined.

    // Check for existence of model_config.xml in current directory (task) and fail if not found.
    if ( !path_exists( MODEL_CONFIG_FILE ) ) {
        std::cerr << ".. DEV NOTE: The model config does not yet exist in the current directory: " << MODEL_CONFIG_FILE << std::endl;
        //GC. Testing only; return finish_task( tstate, 1 );        // should terminate, the model won't run.
    }

    // Create model control instance.
    // In future, rather than pass app_name, we might pass the model name read from model_config.xml.
    // "CPDN" and "fort.4" are placeholders for vendor name and primary control file respectively.
    auto model_ctrl = create_model_control( bconfig.app_name, bconfig.app_version );
    if ( model_ctrl == nullptr ) {
        std::cerr << "..Error creating model control instance. Unsupported model: " << bconfig.app_name << std::endl;
        return finish_task( tstate, 1 );
    }

    // --------------- Argument processing -----------------

    // Long-form arguments.
    ParseResult parse_result = parse_args( argc, argv );
    if ( !parse_result.ok ) {
        return finish_task( tstate, parse_result.exit_code );
    }

    // Process parsed arguments into the data structures used by the rest of the code.
    if ( !process_args( parse_result, tconfig, err_msg ) ) {
        std::cerr << ".." << err_msg << '\n';
        return finish_task( tstate, 1 );
    }

    // Check for optional '--nthreads <value>' at end of arg list optionally set by app_config.xml on user's machine.

    bool using_app_config_nthreads = false;
    int app_config_nthreads = 0;
    if ( !get_app_config_nthreads( argc, argv, app_config_nthreads, using_app_config_nthreads, err_msg ) ) {
        std::cerr << "..Failed to parse --nthreads argument: " << err_msg << '\n';
        return finish_task( tstate, 1 );
    }
    if ( using_app_config_nthreads ) {
        // GC. Enabling this causes the model to deadlock. Not clear why as this just
        //     sets the env variable. I suspect there's some boinc interaction that
        //     prohibits the number of threads to go up. TODO.
        //std::cerr << "Using --nthreads from app_config.xml: " << bconfig.ncpus << '\n';
        //bconfig.ncpus = app_config_nthreads;
        std::cerr << "Note: Ignoring app_config.xml avg_ncpus override. Not currently working.\n";
    }
    std::string nthreads = std::to_string( bconfig.ncpus );    // default or resolved thread count for model launch

    double num_days = 0.0;
    if ( !parse_double_arg( tconfig.filename_fclen, num_days, err_msg ) ) {
        std::cerr << "..Failed to parse --filename_fclen value: " << err_msg << '\n';
        return finish_task( tstate, 1 );
    }

    // --------------- Prepare the task environment -----------------

    boinc_begin_critical_section();

    // Create temp upload folder for moving the results to and uploading the results from.
    // BOINC measures the disk usage on the slots directory so we must move all results out of this folder
    std::string upload_dir = bconfig.project_dir + bconfig.app_name + "_" + tconfig.workunit;
    std::cerr << "Location of temp upload folder: " << upload_dir << '\n';
    if ( !ensure_directory( upload_dir, &err_msg ) ) {
        std::cerr << "..Failed to create temp upload folder for results: " << err_msg << std::endl;
        return finish_task( tstate, 1 );
    }

    //  Unpack application into slot
    retval = move_and_unzip_app_file( bconfig.app_name, bconfig.app_version, bconfig.project_dir, bconfig.slot_path );
    if ( retval ) {
        std::cerr << "..move_and_unzip_app_file failed" << "\n";
        return finish_task( tstate, retval );
    }

    //---------------- Stage & unpack the app bundle ---------------------------
    // This bundle is a BOINC logical input file in the slot. Resolve its jf_* source, copy that source
    // into the slot, and unzip it without overwriting the BOINC logical file itself. If we overwrite
    // the logical file we have no way to recover the original source filename.

    fs::path app_bundle_path = bconfig.slot_path;
    app_bundle_path /= std::string( bconfig.app_name ) + "_" + tconfig.memberid + "_" + tconfig.filename_startdate + "_" +
                       std::to_string( (int)num_days ) + "_" + tconfig.batch + "_" + tconfig.workunit + ".zip";

    auto app_bundle_stage = stage_boinc_input_file( app_bundle_path, bconfig.slot_path, fs::path( "." ), "app_bundle" );
    if ( !app_bundle_stage.ok ) {
        report_input_stage_failure( "app bundle", app_bundle_stage );
        std::cerr << "..App bundle logical path was: " << app_bundle_path.string() << std::endl;
        return finish_task( tstate, 1 );    // should terminate, the model won't run.
    }

    //----------------  Parse the model control file -------------------------------
    // Parse the model control input through the model layer so controller code stays generic.
    // The model control file is expected to get unpacked from the app bundle.

    auto control_input = model_ctrl->parse_control_input();
    if ( !control_input.ok ) {
        report_model_control_input_failure( control_input );
        return finish_task( tstate, 1 );
    }

    tconfig.exptid = control_input.experiment_id;

    const int timestep_seconds = control_input.timestep_seconds;
    const int output_interval = control_input.output_interval;
    int restart_interval_steps = control_input.restart_interval;
    if ( restart_interval_steps < 0 ) {
        restart_interval_steps = abs( restart_interval_steps ) * 3600 / timestep_seconds;
        std::cerr << " NFRRES: restart dump frequency (in steps) " << restart_interval_steps << '\n';
    }

    const int total_steps = control_input.total_steps;
    const int trickle_freq = TrickleHandler::get_trickle_frequency( timestep_seconds, total_steps );
    const double total_length_of_simulation_time = control_input.forecast_length_time;

    std::cerr << "Values read from model control input are: \n"
              << " Experiment ID: " << tconfig.exptid << '\n'
              << " Timestep interval (secs): " << timestep_seconds << '\n'
              << " Frequency of model output (steps): " << output_interval << '\n'
              << " Frequency of restarts/checkpoints (steps): " << restart_interval_steps << '\n'
              << " Total number of model steps: " << total_steps << '\n'
              << " Forecast length: " << total_length_of_simulation_time << '\n';

    std::cerr << "Trickle frequency is every : " << trickle_freq << " model steps, "
              << ( static_cast<double>( trickle_freq ) * static_cast<double>( timestep_seconds ) ) / 86400.0 << " days.\n";

    //---------------- Unpack the remaining model input files -----------------------
    // Unpack through model instance manifest context so main() stays generic.

    model_ctrl->setup_directories( bconfig.slot_path );
    auto input_manifest = model_ctrl->get_input_manifest( tconfig.workunit );
    auto manifest_stage = stage_model_input_manifest( input_manifest, bconfig.slot_path );
    if ( !manifest_stage.ok ) {
        report_input_stage_failure( "model input archive", manifest_stage );
        return finish_task( tstate, 1 );    // should terminate, the model won't run.
    }

    // -------------- Initialise the task state and progress tracking ----------------

    // Initialise the ProgressFile handler
    ProgressFileHandler progress_file( bconfig.slot_path );

    // Check whether the rcf file and the progress file (contains model progress) are not already present from an unscheduled shutdown
    std::cerr << "Checking model's restart file and CPDN progress file: " << progress_file.path() << '\n';

    auto startup_state = initialize_task_state_from_restart( *model_ctrl, progress_file, restart_interval_steps, tstate, err_msg );
    if ( !startup_state.ok ) {
        std::cerr << ".." << err_msg << '\n';
        if ( startup_state.print_model_logs ) {
            model_ctrl->print_logs( 50 );
        }
        return finish_task( tstate, 1 );
    }
    if ( !startup_state.log_message.empty() ) {
        std::cerr << startup_state.log_message;
    }

    tstate.current_step = tstate.last_completed_step;
    tstate.current_cpu_time = tstate.prior_acc_cpu_time;

    // Update progress file with current values
    if ( !progress_file.write( tstate, err_msg ) ) {
        std::cerr << "..Failed to write progress file: " << err_msg << '\n';
        return finish_task( tstate, 1 );
    }

    // upload_interval is controller/task policy in model steps.
    // upload_interval == 0 disables intermediate and final result uploads, but does not disable trickles.
    if ( tconfig.upload_interval < 0 || timestep_seconds <= 0 ) {
        std::cerr << "..upload_interval or timestep_seconds is invalid" << std::endl;
        return finish_task( tstate, 1 );
    }
    if ( tconfig.upload_interval == 0 ) {
        std::cerr << "Result uploads disabled (--upload_interval=0). Trickle messages remain enabled.\n";
    }

    std::cerr << "Total_length_of_simulation_time: " << total_length_of_simulation_time << '\n';

    // Get result_base_name to construct upload file names for both standalone and under BOINC.

    std::string result_base_name = get_result_base_name( bconfig, tconfig );
    std::cerr << "result_base_name: " << result_base_name << '\n';

    // Create the trickle handler (only trickle if not in standalone mode)
    TrickleHandler trickler( bconfig.wu_name, result_base_name, bconfig.slot_path );

    // Check model executable to run.
    // GC. This should be an input parameter on the command line or the init_data.xml (or model_config.xml) later on.

    fs::path model_exe = bconfig.slot_path;
    model_exe /= model_ctrl->get_executable_name();

    if ( !fs::exists( model_exe ) ) {
        std::cerr << ".. Abort. Model executable not found: " << model_exe << std::endl;
        return finish_task( tstate, 1 );
    }

    // Bug workaround. The current cpdn_unzip function does not preserve executable permissions on Linux.
    // Manually set the permissions on the model executable before running.
    // GC. Dec/2025

    if ( !set_exec_perms( model_exe.string() ) ) {
        std::cerr << "..Cannot start model. Setting execute permission for model executable failed: " << model_exe << std::endl;
        return finish_task( tstate, 1 );
    }

    // ------------- EXPERIMENTAL DIAGNOSTICS CODE SETUP ---------------
    // This code is hardwired while I test the use of external diagnostics program
    // The test is Chris O'Reilly's batches where we run a modified sptogp to compute
    // the zonal mean zonal wind.

    fs::path diag_exe = bconfig.slot_path;
    diag_exe /= "diagnostics.exe";

    if ( !fs::exists( diag_exe ) ) {
        std::cerr << " NOTE: No external diagnostics program found. No extra trickle data will be written.";
        diag_exe.clear();    // set to empty path to indicate no diagnostics program. check with diag_exe.empty() before trying to run diagnostics.
    } else {
        // Set execute permissions on diagnostics program if it exists (as above)
        if ( !set_exec_perms( diag_exe.string() ) ) {
            std::cerr << "..WARNING. Will not run diagnostics. Cannot set execute permission for diagnostics program: " << diag_exe << std::endl;
            diag_exe.clear();
        }
    }
    // -- END OF EXPERIMENTAL CODE --

    // --------------- Start the model process -----------------

    std::cerr << "Launching model executable: " << model_exe << std::endl;
    tstate.child_process = launch_process( bconfig.project_dir, bconfig.slot_path, model_exe.string(), nthreads );

    if ( child_process_is_valid( tstate.child_process ) ) {
        tstate.child_status = 0;
    } else {
        std::cerr << "..Error launching model process" << std::endl;
        return finish_task( tstate, 1 );
    }

    boinc_end_critical_section();


    // child_status = 0 running
    // child_status = 1 exited normally
    // child_status = 3 abnormal or forced termination
    // child_status = 4 suspended
    // child_status = 5 child process not found / status unavailable


    //---------------- Main loop------------------------------

    // Periodically check the process status and the BOINC client status

    std::vector<fs::path> zfl;
    BoincRuntime bruntime;

    int delay_count = 0;
    int delay_max = LOOP_DELAY_DEFAULT;

    while ( tstate.child_status == 0 && tstate.model_completed == 0 ) {
        sleep_seconds( 1 );    // Time delay to reduce overhead

        delay_count++;
        refresh_current_cpu_time( tstate );

        // Check whether an upload point has been reached
        // GC. 09/25. reduced to 7 secs as testing shows 10secs can miss a timestep.
        // Going too low can cause the %age done on boincmgr to flip backwards.
        if ( delay_count >= delay_max ) {

            int observed_step = tstate.current_step;
            if ( !model_ctrl->get_current_step( observed_step, total_steps ) ) {
                observed_step = tstate.last_completed_step;
            }
            tstate.current_step = observed_step;

            // Move the model result files to the task folder in the project directory
            // GC. Why do this every timestep? This check only needs to be done at same frequency as NFRPOS.
            // GC. Added run of external diagnostics code if present. EXPERIMENTAL STILL.
            if ( observed_step != tstate.last_completed_step ) {
                auto output_files = model_ctrl->get_output_filenames( observed_step, tconfig.exptid );
                bool diagnostics_ran = run_step_diagnostics( diag_exe, bconfig.slot_path, output_files );

                for ( const auto& result : output_files ) {
                    retval = move_result_file( bconfig.slot_path, upload_dir, result );
                    if ( retval ) {
                        std::cerr << ".. Moving " << result << " result file to the temp folder in the projects directory failed" << "\n";
                        return finish_task( tstate, retval );
                    }
                }

                const double current_step_time = step_to_model_time( observed_step, timestep_seconds );

                // upload_interval == 0 disables result uploads, but trickles still run below.
                // GC. TODO. Why not combine adding to the zip file with moving the result files above?
                if ( tconfig.upload_interval > 0 &&
                     ( ( current_step_time - tstate.last_upload_time ) >= ( static_cast<double>( tconfig.upload_interval ) * timestep_seconds ) ) &&
                     ( current_step_time < total_length_of_simulation_time ) ) {
                    // Create an intermediate results zip file
                    zfl.clear();

                    std::cerr << "End of upload interval reached, starting a new upload process" << std::endl;

                    // *****  Critical section start  *****
                    boinc_begin_critical_section();

                    const int last_upload_step =
                        static_cast<int>( std::llround( tstate.last_upload_time / static_cast<double>( timestep_seconds ) ) );

                    // Cycle through all the completed steps from the last upload point up to the current interval boundary.
                    for ( int step_to_zip = last_upload_step; step_to_zip < observed_step; ++step_to_zip ) {

                        // Add model result files to zip to be uploaded
                        for ( const auto& result : model_ctrl->get_output_filenames( step_to_zip, tconfig.exptid ) ) {
                            fs::path fpath = upload_dir;
                            fpath /= result;
                            if ( fs::exists( fpath ) ) {
                                std::cerr << "Adding to the zip: " << fpath << '\n';
                                zfl.push_back( fpath );
                            }
                        }
                    }

                    if ( !zfl.empty() ) {
                        std::string upload_file_name = "upload_file_" + std::to_string( tstate.upload_file_number ) + ".zip";
                        if ( !bconfig.standalone ) {
                            std::cerr << "Uploading the intermediate file: " << upload_file_name << '\n';
                        }

                        auto upload_result = zip_and_send_upload( bconfig, bruntime, tstate, result_base_name, tstate.upload_file_number, zfl );
                        if ( !upload_result.ok ) {
                            report_upload_send_failure( "intermediate upload", upload_result );
                            return finish_task( tstate, upload_result.finish_code );
                        }
                        if ( upload_result.upload_attempted ) {
                            std::cerr << "Finished the upload of the intermediate file: " << upload_file_name << '\n';
                        }
                        tstate.last_upload_time = current_step_time;
                    }
                    tstate.last_upload_time = current_step_time;

                    // *****  Normal end of critical section  *****
                    boinc_end_critical_section();
                    tstate.upload_file_number++;

                }    // end of upload new output file block.

                // Trickle every required fraction of the model run
                if ( trickle_freq > 0 ) {
                    if ( ( observed_step % trickle_freq ) == 0 ) {
                        std::cerr << "Sending progress trickle message to CPDN at step: " << observed_step << '\n';
                        trickler.process_trickle( tstate.current_cpu_time, observed_step );
                        tstate.last_trickle_step = observed_step;
                    }
                }

                tstate.last_completed_step = observed_step;
                delay_max = diagnostics_ran ? LOOP_DELAY_FAST : LOOP_DELAY_DEFAULT;
            } else {
                delay_max = LOOP_DELAY_DEFAULT;
            }    // end of if it's a new timestep block.

            delay_count = 0;

            // Update progress file with current values
            if ( !progress_file.write( tstate, err_msg ) ) {
                std::cerr << "..Failed to write progress file: " << err_msg << '\n';
                return finish_task( tstate, 1 );
            }
        }

        // Calculate the fraction done
        tstate.fraction_done = model_frac_done( static_cast<double>( tstate.current_step ), static_cast<double>( total_steps ), bconfig.ncpus );

        // If the current model step is a restart interval, update restart cpu time for boinc reporting.
        if ( !( tstate.current_step % restart_interval_steps ) ) {
            tstate.restart_cpu_time = tstate.current_cpu_time;
        }

        // Note boinc fns themselves will check standalone but for safety we do it here too.
        if ( !bconfig.standalone ) {
            // According to the boinc wrapper code example, the cpu time reported is for the current
            // process since restart (or first run), whereas the restart/checkpoint cpu time & fraction done are for the whole run.
            boinc_report_app_status( tstate.current_cpu_time, tstate.restart_cpu_time + tstate.prior_acc_cpu_time, tstate.fraction_done );

            // Provide the fraction done to the BOINC client, necessary for the percentage bar on the client
            boinc_fraction_done( tstate.fraction_done );

            boinc_get_status( &bruntime.client_status );
            (void)handle_boinc_client_status( tstate.child_process, bruntime );    // Child status is refreshed immediately below.
        }

        tstate.child_status = check_child_status( tstate.child_process, tstate.child_status, tstate.exit_code );
    }

    //--------- End of main loop ---------

    // -------- Task cleanup & final uploads -----------

    // Do NOT execute a return until the final upload is done after the boinc_end_critical_section() below.

    // GC. I probably don't need this; use the child_status variable and model_success instead in main loop?
    tstate.model_completed = 1;

    // Time delay to ensure model files are all flushed to disk
    std::cerr << "Waiting for file operations to complete...(60 secs)" << std::endl;
    if ( !sleep_with_boinc_poll( bruntime, bconfig.standalone, 60 ) ) {
        if ( !handle_boinc_client_status( tstate.child_process, bruntime ) ) {
            return finish_task( tstate, get_task_finish_code( tstate, bruntime ) );
        }
    }

    tstate.model_success = model_ctrl->check_model_success();

    if ( tstate.model_success ) {
        std::cerr << "..Model completed successfully" << std::endl;
    } else {
        std::cerr << "..Failed, model did not complete successfully" << std::endl;
        std::cerr << "..Model exit code: " << tstate.exit_code << std::endl;
    }

    // Print the model logs & progress file (if they exist)
    std::cerr << ".. Printing tail of model log files .." << std::endl;
    model_ctrl->print_logs( 40 );
    std::cerr << "... Printing controller progress file .. " << std::endl;
    progress_file.print( std::cerr );


    //---------------- Create the final results zip file-------------------

    // Although the final move of output files may have failed above, there might still be some previous
    // output files in the upload dir ready to be zipped and uploaded.

    boinc_begin_critical_section();

    // Move the final model result files ready for upload
    auto output_files = model_ctrl->get_output_filenames( tstate.last_completed_step, tconfig.exptid );
    run_step_diagnostics( diag_exe, bconfig.slot_path, output_files );
    for ( const auto& result : output_files ) {
        retval = move_result_file( bconfig.slot_path, upload_dir, result );
        if ( retval ) {
            std::cerr << "..Copying " << result << " model output file to the temp upload folder in projects directory failed" << "\n";
        }
    }

    if ( tconfig.upload_interval > 0 ) {
        zfl.clear();

        // Add the model log files to the final upload
        for ( const auto& logfile : model_ctrl->get_log_filenames() ) {
            fs::path logpath = bconfig.slot_path;
            logpath /= logfile;
            if ( fs::exists( logpath ) ) {
                zfl.push_back( logpath.string() );
                std::cerr << "Adding model log file to the upload zipfile: " << logpath << '\n';
            }
        }

        // Read the remaining list of files from the temp upload directory and
        // add the matching files to the upload zip
        retval = add_upload_files( upload_dir, zfl, model_ctrl->get_output_filename_regex() );
        if ( retval ) {
            std::cerr << "Adding model output files to the upload zip failed!\n";
        }

        if ( !zfl.empty() ) {
            std::string upload_file_name = "upload_file_" + std::to_string( tstate.upload_file_number ) + ".zip";
            if ( !bconfig.standalone ) {
                std::cerr << "Uploading the final file: " << upload_file_name << '\n';
            }

            auto upload_result = zip_and_send_upload( bconfig, bruntime, tstate, result_base_name, tstate.upload_file_number, zfl );
            if ( !upload_result.ok ) {
                report_upload_send_failure( "final upload", upload_result );
                return finish_task( tstate, upload_result.finish_code );
            }
            if ( upload_result.upload_attempted ) {
                std::cerr << "Finished the upload of the final file" << '\n';
            }
        }
    }

    // upload_interval == 0 disables result uploads, but trickles remain enabled.
    if ( !bconfig.standalone && tstate.current_step > tstate.last_trickle_step ) {
        refresh_current_cpu_time( tstate );
        trickler.process_trickle( tstate.current_cpu_time, tstate.current_step );
    }

    //---------------- Cleanup ---------------------------------------

    // Remove the temp folder
    fs::remove_all( upload_dir );

    boinc_end_critical_section();

    // Delay to ensure all files are flushed to disk before exiting
    std::cerr << "Waiting for file operations to complete...(90 secs)" << std::endl;
    if ( !sleep_with_boinc_poll( bruntime, bconfig.standalone, 90 ) ) {
        if ( !handle_boinc_client_status( tstate.child_process, bruntime ) ) {
            return finish_task( tstate, get_task_finish_code( tstate, bruntime ) );
        }
    }

    std::cerr << "Task finished." << std::endl;
    return finish_task( tstate, get_task_finish_code( tstate, bruntime ) );
}

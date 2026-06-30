//
// BOINC task controller for CPDN.
//
// This version written by Glenn Carver, CPDN, 2025->
// Complete rewrite of original version by Andy Bowery (OERC) December 2023.
//
// AI assistance from : GPT-5.3, GPT-5.4 and GPT-5.5.
//

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#include "boinc/boinc_api.h"
#include "boinc/error_numbers.h"

#include "control_start.h"
#include "cpdn_control.h"
#include "lib/cpdn_cpu_time.h"
#include "lib/logging_utils.h"
#include "lib/utils.h"
#include "parse_args.h"
#include "upload_manager.h"

#include "api/model_control.h"
#include "api/progressfile_handler.h"
#include "api/trickle_handler.h"

#include "models/openifs/oifs_control.h"
#include "models/wrf/wrf_control.h"

namespace chrono = std::chrono;
namespace fs = std::filesystem;

namespace {

void log_boinc_api_error( const char* api_name, int retval )
{
    std::cerr << "Error in boinc : " << api_name << " failed (" << retval << "): " << boincerror( retval ) << '\n';
}

}    // namespace


// Define the code version if not defined at compile time with -D option.
#ifndef CODE_VERSION
#define CODE_VERSION "1.0.0"
#endif


// Constants
constexpr std::string_view MODEL_CONFIG_FILE = "model_config.xml";    // not in use (yet?)
constexpr int LOOP_DELAY_DEFAULT = 5;                                 // secs


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
static std::unique_ptr<ModelControl> create_model_control( std::string_view model_name, std::string_view app_version )
{
    std::unique_ptr<ModelControl> model;    // create a null unique_ptr ready for a new model control instance.

    // Model mappings
    // As the test model is an OpenIFS skeleton clone, we use the OpenIFSControl class.

    if ( model_name == "test_model" ) {
        model = std::make_unique<OpenIFSControl>( "CPDN", model_name, app_version, "test_model" );

    } else if ( model_name == "oifs_43r3_omp_l159" || model_name == "oifs_43r3_omp_l319" || model_name == "oifs_43r3_parest_omp_l319" ) {
        model = std::make_unique<OpenIFSControl>( "ECMWF", model_name, app_version, "oifs_43r3_omp_model.exe" );

    } else if ( model_name == "wrf_4.6.1_urban" ) {
        model = std::make_unique<WRFControl>( "UCAR", model_name, app_version, "wrf_4.6.1_urban.exe" );

    } else {
        std::cerr << "Unsupported model. Name : " << model_name << ", app version : " << app_version << '\n';
    }

    return model;
}


/**
 * @brief Report failure to stage an input file with as much context as possible.
 *        Includes diagnostic hints for common failure modes (e.g., unresolved logical files).
 */
static void report_input_stage_failure( std::string_view context, const InputStageResult& result )
{
    std::cerr << "Failed to stage " << context;
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

    // Provide diagnostic hint if logical file was not resolved to a project archive
    if ( !result.logical_file.empty() && !result.resolved_project_file.empty() && result.resolved_project_file == result.logical_file ) {
        std::cerr << "   DIAGNOSTIC HINT: The logical file was NOT resolved to a project (jf_*) archive.\n";
        std::cerr << "   This typically means: soft link XML has malformed tags (e.g., <softlink> instead of <soft_link>),\n";
        std::cerr << "   missing/corrupted soft link file, or BOINC resolution failure.\n";
    }
}


/**
 * @brief Report failure to parse the model control input with as much context as possible.
 */
static void report_model_control_input_failure( const ModelControlInputData& result )
{
    std::cerr << "Failed to parse model control input";
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

    int requested_nthreads = -1;

    std::string nthreads_value = argv[argc - 1];
    if ( !parse_int( nthreads_value, requested_nthreads, err_msg ) ) {
        std::cerr << "Warning. --nthreads argument must be a valid integer! " << err_msg << '\n';
        return false;
    }

    used_app_config_nthreads = true;
    nthreads = requested_nthreads;

    return true;
}


/**
 * @brief Checks nthreads value against what the model allows
 * @param nthreads Requested number of threads, may be changed on exist
 * @returns True if nthreads was changed, otherwise false.
 */
static bool check_model_nthreads( int& nthreads, const ModelControl& model_ctrl )
{
    bool changed = false;

    int model_min_threads = 1;
    int model_max_threads = 1;
    model_ctrl.get_nthreads_range( model_min_threads, model_max_threads );

    if ( nthreads > model_max_threads ) {
        std::cerr << "Warning. Number of threads too high. Setting to max allowed threads : " << model_max_threads << '\n';
        nthreads = model_max_threads;
        changed = true;
    } else if ( nthreads < model_min_threads ) {
        std::cerr << "Warning. Number of threads too low. Setting to min allowed threads : " << model_min_threads << '\n';
        nthreads = model_min_threads;
        changed = true;
    }
    return changed;
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
            log_boinc_api_error( "boinc_resolve_filename_s", retval );
            return base_name;
        }

        base_name = fs::path( resolved_name ).stem().string();    // returns filename without path nor '.zip'
        if ( base_name.length() > 2 ) {
            base_name.erase( base_name.length() - 2 );    // remove the '_0'
        }
        if ( base_name.compare( "upload_file" ) == 0 ) {
            std::cerr << "Failed to get result name" << std::endl;
            return base_name;
        }
    } else {
        base_name = bconfig.app_name + "_" + tconfig.filename_startdate + "_" + tconfig.batch + "_" + tconfig.workunit;
    }
    return base_name;
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


enum class ShutdownReason { boinc_client_shutdown, controller_error };


static void terminate_child_for_shutdown( TaskState& tstate )
{
    bool child_termination_requested = false;
    std::string child_cleanup_err;
    if ( !terminate_child_process_if_active( tstate.child_process, tstate.child_status, child_termination_requested, child_cleanup_err ) ) {
        std::cerr << "Failed to terminate active child process during task shutdown: " << child_cleanup_err << '\n';
    } else if ( child_termination_requested ) {
        std::cerr << "Task shutdown requested while child process is still active; terminating child process before boinc_finish()\n";
    }
}


/**
 * @brief Cleanup and finish the task.
 *        Closes any child-process handle state, ends any BOINC critical section,
 *        then calls boinc_finish() and returns the same exit code.
 *        boinc_finish exits under BOINC, but return is kept for dummy libraries.
 */
static int finish_task( TaskState& tstate, int exit_code )
{
    terminate_child_for_shutdown( tstate );
    close_child_process_handle( tstate.child_process );
    boinc_end_critical_section();    // in case we abort while in critical section (boinc api handles case if not in critical section).
    boinc_finish( exit_code );       // boinc_finish exits, no further code executed after this call (unless a dummy library is used).
    return exit_code;
}


static int shutdown_task( TaskState& tstate, int exit_code, const ShutdownReason reason, UploadManager* upload_manager, BoincRuntime* runtime )
{
    if ( reason == ShutdownReason::controller_error && upload_manager != nullptr && runtime != nullptr ) {
        terminate_child_for_shutdown( tstate );

        std::cerr << "Waiting for file operations to complete...(60 secs)" << std::endl;
        if ( !sleep_with_boinc_poll( *runtime, upload_manager->standalone(), 60 ) ) {
            if ( runtime->client_status.quit_request || runtime->client_status.abort_request || runtime->client_status.no_heartbeat ) {
                return finish_task( tstate, get_task_finish_code( tstate, *runtime ) );
            }
        }

        auto upload_result = upload_manager->finalize_remaining_uploads( *runtime, tstate, tstate.last_completed_step, true, false );
        if ( !upload_result.ok ) {
            return finish_task( tstate, upload_result.finish_code );
        }
        upload_manager->cleanup_upload_dir();
    }

    return finish_task( tstate, exit_code );
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
        std::cerr << "BOINC initialisation failed" << "\n";
        return finish_task( tstate, retval );
    }
    if ( bconfig.slot_path.empty() ) {
        std::cerr << "Error. Can't determine slot path: current_path() returned empty" << std::endl;
        return finish_task( tstate, 1 );
    }

    // Install temporary streambuf wrapper on std::cerr so each new log line gets
    // a date/time prefix automatically. This keeps the existing stream-style logging
    // code intact while making remote stderr logs easier to correlate and debug.
    Timestamped timestamped_cerr( std::cerr );

    // Print banner with key config & model information.
    banner( bconfig, CODE_VERSION );
    if ( bconfig.standalone ) {
        std::cerr << "Running in standalone mode" << '\n';
    }

    // ---------------- Task configuration -----------------

    // Read in the model config.xml.  The XML file contains all the information
    // about the model. It's required to initialize the correct model class later on.
    // GlennC. May not need this in the future. to be determined.

    // Check for existence of model_config.xml in current directory (task) and fail if not found.
    //if ( !path_exists( MODEL_CONFIG_FILE ) ) {
    //    std::cerr << " DEV NOTE: The model config does not yet exist in the current directory: " << MODEL_CONFIG_FILE << std::endl;
    //    //GC. Testing only; return finish_task( tstate, 1 );        // should terminate, the model won't run.
    //}

    // Create model control instance.
    // In future, rather than pass app_name, we might pass the model name read from model_config.xml.
    // "CPDN" and "fort.4" are placeholders for vendor name and primary control file respectively.
    auto model_ctrl = create_model_control( bconfig.app_name, bconfig.app_version );
    if ( model_ctrl == nullptr ) {
        std::cerr << "Error creating model control instance. Unsupported model: " << bconfig.app_name << std::endl;
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
        std::cerr << err_msg << '\n';
        return finish_task( tstate, 1 );
    }

    // -------------- <app_config.xml> options processing ----
    // Check for optional '--nthreads <value>' args on the command line; optionally set by users's app_config.xml.

    bool using_app_config_nthreads = false;
    int app_config_nthreads = 0;

    if ( !get_app_config_nthreads( argc, argv, app_config_nthreads, using_app_config_nthreads, err_msg ) ) {
        std::cerr << "Failed to parse --nthreads argument: " << err_msg << '\n'
                  << "Expected usage: --nthreads <integer value>\n"
                  << "Ignoring app_config.xml and using default nthreads value.\n";
        using_app_config_nthreads = false;
    }

    if ( using_app_config_nthreads ) {
        std::cerr << "Using --nthreads from app_config.xml: " << bconfig.ncpus << '\n';
        check_model_nthreads( app_config_nthreads, *model_ctrl );
        bconfig.ncpus = app_config_nthreads;
    }
    std::string nthreads = std::to_string( bconfig.ncpus );

    // TODO. This should be in process_args??
    double num_days = 0.0;
    if ( !parse_double_arg( tconfig.filename_fclen, num_days, err_msg ) ) {
        std::cerr << "Failed to parse --filename_fclen value: " << err_msg << '\n';
        return finish_task( tstate, 1 );
    }

    // --------------- Prepare the task environment -----------------

    boinc_begin_critical_section();

    // Create temp upload folder to move the results into and upload from.
    // BOINC measures the disk usage on the slots directory so we must move all results out of this folder
    fs::path upload_dir = fs::path( bconfig.project_dir ) / ( bconfig.app_name + "_" + tconfig.workunit );

    std::cerr << "Location of upload folder in project directory: " << upload_dir << '\n';
    if ( !ensure_directory( upload_dir, &err_msg ) ) {
        std::cerr << "Failed to create temp upload folder for results: " << err_msg << std::endl;
        return finish_task( tstate, 1 );
    }

    //  ------------- Info: Naming and content of download applications and workunit files -----------------

    //  The cpdn_control executable is a non-zipped binary file downloaded from main.cpdn.org/applications.
    //  The model application, with any other executables (e.g. diagnostics.exe) is a zipped file
    //  also downloaded from main.cpdn.org/applications as a BOINC logical file.
    //
    //  The workunit data files are downloaded from main.cpdn.org/download/batch_<no>
    //    ../ancils  e.g. for OIFS : clim_data_<wu>.zip, ic_ancil_<wu>.zip and ifsdata_<wu>.zip.
    //               The ic_ancil.zip file for OIFS contains the workunit initial conditions.
    //    ../workunits
    //       This contains the workunit input control data. e.g. for OIFS, fort.4 and wam_namelist.
    //       Each filename is : <app_name>_<memberid>_<startdate>_<fclen>_<batch>_<workunit>.zip.
    //
    //  Different models will use a different number of files.
    //
    //  The boinc client will put the cpdn_control executable into the slot directory. All other files
    //  will be downloaded as BOINC logical files (jf_*) into the project directory to be copied into the slot.
    //
    //  Each logical file (e.g. ic_ancil_<wu>.zip) contains a single line pointing to the downloaded real file
    //  in the project dir. Via a call to boinc_resolve_filename() we can get the real file path (jf_<md5sum>,
    //  verify the md5sum of the file, and stage the file (copy into the slot and unzip).
    //
    //  Note the logical file is not overwritten. If we overwrite the logical file we have no way
    //  to recover the original source filename on a restart if something goes wrong.

    //  -------------- Unpack application executables zipfile into slot ------------------
    //  This will be the actual model executable and any accompanying executables (e.g. diagnostics.exe).

    std::cerr << " Staging application executable(s) into slot.." << '\n';
    retval = stage_and_unzip_app_file( bconfig.app_name, bconfig.app_version, bconfig.project_dir, bconfig.slot_path );
    if ( retval ) {
        std::cerr << "stage_and_unzip_app_file failed" << '\n';
        return finish_task( tstate, retval );
    }

    //---------------- Stage (copy into slot & unpack) the workunit file(s) zip ---------------------------
    // This typically contains the model control file(s) (e.g. namelists).

    fs::path app_bundle_path = bconfig.slot_path;
    app_bundle_path /= std::string( bconfig.app_name ) + "_" + tconfig.memberid + "_" + tconfig.filename_startdate + "_" +
                       std::to_string( (int)num_days ) + "_" + tconfig.batch + "_" + tconfig.workunit + ".zip";

    std::cerr << " Staging workunit files zipfile into slot.." << '\n';
    auto app_bundle_stage = stage_boinc_input_file( app_bundle_path, bconfig.slot_path, fs::path( "." ), "app_bundle" );
    if ( !app_bundle_stage.ok ) {
        report_input_stage_failure( "app bundle", app_bundle_stage );
        std::cerr << "App bundle logical path was: " << app_bundle_path.string() << std::endl;
        return finish_task( tstate, 1 );    // should terminate, the model won't run.
    }

    //---------------- Stage (copy & unpack) remaining model input files -----------------------
    // Unpack through model instance manifest context so main() stays generic.

    // Do this before setup() so unpacking works correctly (may be a null op)
    model_ctrl->setup_directories( bconfig.slot_path );

    std::cerr << " Staging remaining workunit model zipfiles into slot.." << '\n';
    auto input_manifest = model_ctrl->get_input_manifest( tconfig.workunit );
    auto manifest_stage = stage_model_input_manifest( input_manifest, bconfig.slot_path );
    if ( !manifest_stage.ok ) {
        report_input_stage_failure( "model input archive", manifest_stage );
        return finish_task( tstate, 1 );    // should terminate, the model won't run.
    }

    //----------------  Ask model to run it's own setup before the model control file (e.g. namelist) is parsed.
    // This allows the model to do any necessary edits before we ask it to parse the control file.
    // For example, WRF restart flag might need to be reset if restart files are present.

    if ( !model_ctrl->setup( bconfig.slot_path ) ) {
        std::cerr << "Model setup failed.\n";
        return finish_task( tstate, 1 );
    }

    //----------------  Ask model for key control variables needed to manage the task  -------------------------------

    // Parse the model control file (e.g. namelist) through the model layer so controller code stays generic.
    // The model control file is expected to get unpacked from the app bundle.

    auto control_input = model_ctrl->parse_control_input();
    if ( !control_input.ok ) {
        report_model_control_input_failure( control_input );
        return finish_task( tstate, 1 );
    }

    const int timestep_seconds = control_input.timestep_seconds;
    int restart_interval_steps = control_input.restart_interval;

    const int total_steps = control_input.total_steps;
    const int trickle_freq = TrickleHandler::get_trickle_frequency( timestep_seconds, total_steps );

    std::cerr << "Trickle frequency is every : " << trickle_freq << " model steps, "
              << ( static_cast<double>( trickle_freq ) * static_cast<double>( timestep_seconds ) ) / 86400.0 << " days.\n";


    // -------------- Initialise the task state and progress tracking ----------------

    // Initialise the ProgressFile handler
    ProgressFileHandler progress_file( bconfig.slot_path );

    // Check if a model restart file and progress file are not already present from an unscheduled shutdown
    std::cerr << "Checking model's restart file and CPDN progress file: " << progress_file.path() << '\n';

    auto startup_state = initialize_task_state_from_restart( *model_ctrl, progress_file, restart_interval_steps, tstate, err_msg );
    if ( !startup_state.ok ) {
        std::cerr << err_msg << '\n';
        if ( startup_state.print_model_logs ) {
            model_ctrl->print_logs( 50 );
        }
        return finish_task( tstate, 1 );
    }
    if ( !startup_state.log_message.empty() ) {
        std::cerr << startup_state.log_message;
    }

    if ( tstate.model_completed != 0 ) {
        std::cerr << "Resetting stale model_completed state from progress file before relaunch: " << tstate.model_completed << '\n';
    }
    prepare_task_state_for_controller_run( tstate );

    // Update progress file with current values
    if ( !progress_file.write( tstate, err_msg ) ) {
        std::cerr << "Failed to write progress file: " << err_msg << '\n';
        return finish_task( tstate, 1 );
    }

    // upload_interval is controller/task policy in model steps.
    // upload_interval == 0 disables intermediate and final result uploads, but does not disable trickles.
    if ( tconfig.upload_interval < 0 || timestep_seconds <= 0 ) {
        std::cerr << "upload_interval or timestep_seconds is invalid" << std::endl;
        return finish_task( tstate, 1 );
    }
    if ( tconfig.upload_interval == 0 ) {
        std::cerr << "Result uploads disabled (--upload_interval=0). Trickle messages remain enabled.\n";
    }

    // Get result_base_name to construct upload file names for both standalone and under BOINC.

    std::string result_base_name = get_result_base_name( bconfig, tconfig );
    std::cerr << "result_base_name: " << result_base_name << '\n';

    // Create the upload manager; all main loop and final loop uploads go through this class
    UploadManager upload_manager( bconfig, *model_ctrl, upload_dir, result_base_name, total_steps, tconfig.upload_interval );

    // Create the trickle handler (only trickle if not in standalone mode)
    TrickleHandler trickler( bconfig.wu_name, result_base_name, bconfig.slot_path );
    BoincRuntime bruntime;

    // Check model executable to run.
    // GC. This could be an input parameter on the command line or the init_data.xml (or model_config.xml) later on.

    fs::path model_exe = bconfig.slot_path;
    model_exe /= model_ctrl->get_executable_name();

    if ( !fs::exists( model_exe ) ) {
        std::cerr << " Abort. Model executable not found: " << model_exe << std::endl;
        return finish_task( tstate, 1 );
    }

    // Bug workaround. The current cpdn_unzip function does not preserve executable permissions on Linux.
    // Manually set the permissions on the model executable before running.
    // GC. Dec/2025

    if ( !set_exec_perms( model_exe.string() ) ) {
        std::cerr << "Cannot start model. Setting execute permission for model executable failed: " << model_exe << std::endl;
        return finish_task( tstate, 1 );
    }

    // --------------- Start the model process -----------------
    // child_status = 0 running
    // child_status = 1 exited normally
    // child_status = 3 abnormal or forced termination
    // child_status = 4 suspended
    // child_status = 5 child process not found / status unavailable

    std::cerr << "Launching model executable: " << model_exe << std::endl;
    tstate.child_process = launch_process( *model_ctrl, bconfig.project_dir, bconfig.slot_path, model_exe.string(), nthreads );

    if ( child_process_is_valid( tstate.child_process ) ) {
        tstate.child_status = 0;
    } else {
        std::cerr << "Error launching model process" << std::endl;
        return finish_task( tstate, 1 );
    }

    boinc_end_critical_section();


    //---------------- Main loop ------------------------------

    // The main loop carries out the following tasks:
    // - Periodically check the process status and the BOINC client status
    // - BOINC housekeeping; updating fraction done
    // - Get latest model timestep from model log
    // - If timestep has changed, carry out tasks:
    //    - run the model's own step related tasks
    //    - if required, move the latest model output to the project dir
    //    - if required, prepare and commit an upload

    double loop_delay_seconds = static_cast<double>( LOOP_DELAY_DEFAULT );
    double next_delay_seconds = loop_delay_seconds;

    std::cerr << "Entering main loop with child_status=" << tstate.child_status << ", model_completed=" << tstate.model_completed
              << ", current_step=" << tstate.current_step << ", last_completed_step=" << tstate.last_completed_step << '\n';

    while ( tstate.child_status == 0 && tstate.model_completed == 0 ) {

        // Wait for short period to avoid excessive filesystem activity.
        if ( !sleep_with_boinc_poll( bruntime, bconfig.standalone, next_delay_seconds ) &&
             !handle_boinc_client_status( tstate.child_process, bruntime ) ) {
            return shutdown_task( tstate, get_task_finish_code( tstate, bruntime ), ShutdownReason::boinc_client_shutdown, &upload_manager,
                                  &bruntime );
        }

        next_delay_seconds = loop_delay_seconds;
        refresh_current_cpu_time( tstate );

        // Refresh child status before reading the latest model step so a final completed
        // step logged just before process exit can still be handled in this iteration.
        tstate.child_status = check_child_status( tstate.child_process, tstate.child_status, tstate.exit_code );

        //  Update our knowledge of what time step the model has got to
        int observed_step = tstate.last_completed_step;
        if ( model_ctrl->get_current_step( observed_step, total_steps ) ) {
            tstate.current_step = observed_step;
        }

        // If the model step has updated, carry out various tasks.
        if ( observed_step != tstate.last_completed_step ) {

            std::cerr << "Main loop. Current observed step: " << tstate.current_step << ", last step: " << tstate.last_completed_step << '\n';

            //  1:  Ask the model to do its own tasks on a step change.
            //  This can involve running a separate external diagnostics executable to create trickle data, or,
            //  some cleanup of restarts for example. Time it as it might be a significant delay.

            const auto step_start = chrono::steady_clock::now();
            (void)model_ctrl->do_step_tasks( observed_step, bconfig.slot_path );
            const double step_elapsed = chrono::duration<double>( chrono::steady_clock::now() - step_start ).count();
            next_delay_seconds = std::max( 0.0, loop_delay_seconds - step_elapsed );


            //  2:  Ask the model which output files are currently safe to copy out of the slot directory.
            //      This controller seam intentionally asks for all files safe to copy as of the current timestep.
            //      main() must not infer readiness from a step->filename mapping because models differ in how
            //      many timesteps contribute to a file and when a file becomes complete.

            auto copyable_output_files = model_ctrl->get_copyable_output_filenames( observed_step );
            upload_manager.move_copyable_output_files( observed_step );

            //  3:  Process upload if required.

            auto scheduled_upload_result = upload_manager.process_scheduled_upload( bruntime, tstate, observed_step );
            if ( !scheduled_upload_result.ok ) {
                return finish_task( tstate, scheduled_upload_result.finish_code );
            }

            //  4:  Trickle every required fraction of the model run

            if ( trickle_freq > 0 && ( observed_step % trickle_freq ) == 0 ) {
                std::cerr << "Sending progress trickle message to CPDN at step: " << observed_step << '\n';
                trickler.process_trickle( tstate.current_cpu_time, observed_step );
                tstate.last_trickle_step = observed_step;
            }

            tstate.last_completed_step = observed_step;

        }    // end of if observed step changed

        //  5: Update progress file with current values
        if ( !progress_file.write( tstate, err_msg ) ) {
            std::cerr << "Failed to write progress file: " << err_msg << '\n';
            return shutdown_task( tstate, 1, ShutdownReason::controller_error, &upload_manager, &bruntime );
        }

        //  6:  BOINC client housekeeping tasks
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
            (void)handle_boinc_client_status( tstate.child_process, bruntime );
        }
    }

    //--------- End of main loop ---------

    // -------- Allow the model controller to do any final work before we do our final steps -------

    if ( !model_ctrl->finalize( bconfig.slot_path ) ) {
        std::cerr << "Model finalize() function reported failure.\n";
        // Not a critical failure so continue with final steps.
    }

    // -------- Task cleanup & final uploads -----------

    // Do NOT execute a return until the final upload is done after the boinc_end_critical_section() below.

    tstate.model_completed = 1;    // completed does not mean it ran ok!
    if ( !progress_file.write( tstate, err_msg ) ) {
        std::cerr << "Warning. Failed to write final progress file: " << err_msg << '\n';    // not a critical error.
    }

    // Time delay to ensure model files are all flushed to disk
    std::cerr << "Waiting for file operations to complete...(60 secs)" << std::endl;
    if ( !sleep_with_boinc_poll( bruntime, bconfig.standalone, 60 ) && !handle_boinc_client_status( tstate.child_process, bruntime ) ) {
        return shutdown_task( tstate, get_task_finish_code( tstate, bruntime ), ShutdownReason::boinc_client_shutdown, &upload_manager, &bruntime );
    }

    tstate.model_success = model_ctrl->check_model_success();

    if ( tstate.model_success ) {
        std::cerr << " Model completed successfully" << std::endl;
    } else {
        std::cerr << " Failed, model did not complete successfully" << std::endl;
        std::cerr << " Model exit code: " << tstate.exit_code << std::endl;
    }

    // Print the model logs & progress file (if they exist)
    std::cerr << " Printing tail of model log files (if any) .." << std::endl;
    model_ctrl->print_logs( 40 );
    std::cerr << " Printing controller progress file .. " << std::endl;
    progress_file.print( std::cerr );

    auto final_upload_result = upload_manager.finalize_remaining_uploads( bruntime, tstate, tstate.last_completed_step, true );
    if ( !final_upload_result.ok ) {
        return finish_task( tstate, final_upload_result.finish_code );
    }

    // upload_interval == 0 disables result uploads, but trickles remain enabled.
    if ( !bconfig.standalone && tstate.current_step > tstate.last_trickle_step ) {
        refresh_current_cpu_time( tstate );
        trickler.process_trickle( tstate.current_cpu_time, tstate.current_step );
    }

    //---------------- Cleanup ---------------------------------------

    // Remove the temp folder
    upload_manager.cleanup_upload_dir();

    // Delay to ensure all files are flushed to disk before exiting
    std::cerr << "Waiting for file operations to complete...(60 secs)" << std::endl;
    if ( !sleep_with_boinc_poll( bruntime, bconfig.standalone, 60 ) ) {
        if ( !handle_boinc_client_status( tstate.child_process, bruntime ) ) {
            return shutdown_task( tstate, get_task_finish_code( tstate, bruntime ), ShutdownReason::boinc_client_shutdown, &upload_manager,
                                  &bruntime );
        }
    }

    std::cerr << "Task finished." << std::endl;
    return finish_task( tstate, get_task_finish_code( tstate, bruntime ) );
}

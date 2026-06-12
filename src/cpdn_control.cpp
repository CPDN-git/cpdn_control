//
// Controller functions for CPDN BOINC model applications.
//
//    Glenn Carver, CPDN, 2025.
//
// Rewritten into class structure and modular form: Glenn Carver (CPDN), 2025->
// Original code: Andy Bowery (Oxford eResearch Centre, Oxford University) May 2023
//

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#include <cstdio>
#include <cstdlib>

#include "boinc/boinc_api.h"
#include "boinc/diagnostics.h"
#include "boinc/md5_file.h"
#include "boinc/util.h"

#include "cpdn_control.h"
#include "cpdn_zip.h"
#include "lib/utils.h"
#include "api/model_control.h"

namespace chrono = std::chrono;
namespace fs = std::filesystem;

#ifndef CPDN_PLATFORM
#define CPDN_PLATFORM "unknown-platform"
#endif

static std::string lowercase_copy( std::string value )
{
    std::transform( value.begin(), value.end(), value.begin(), []( unsigned char ch ) { return static_cast<char>( std::tolower( ch ) ); } );
    return value;
}

static bool extract_expected_md5( const fs::path& project_file, std::string& expected_md5 )
{
    std::string filename = project_file.filename().string();
    if ( filename.size() != 35 || filename.rfind( "jf_", 0 ) != 0 ) {
        return false;
    }

    expected_md5 = filename.substr( 3 );
    if ( !std::all_of( expected_md5.begin(), expected_md5.end(), []( unsigned char ch ) { return std::isxdigit( ch ) != 0; } ) ) {
        expected_md5.clear();
        return false;
    }

    expected_md5 = lowercase_copy( expected_md5 );
    return true;
}

static InputStageResult make_stage_success()
{
    InputStageResult result;
    result.ok = true;
    return result;
}

static InputStageResult make_stage_error( std::string_view step, std::string message )
{
    InputStageResult result;
    result.ok = false;
    result.step = step;
    result.message = std::move( message );
    return result;
}

static int child_status_from_process_state( ChildProcessState process_state, int current_status )
{
    switch ( process_state ) {
    case ChildProcessState::running:
        return 0;
    case ChildProcessState::exited:
        return 1;
    case ChildProcessState::terminated:
        return 3;
    case ChildProcessState::suspended:
        return 4;
    case ChildProcessState::unavailable:
        return 5;
    default:
        return current_status;
    }
}


/**
 * @brief Initialise BOINC data structure and set the options
 */
int init_boinc( BoincConfig& config )
{

    //boinc_init_diagnostics(BOINC_DIAG_DEFAULTS);
    boinc_init();
    boinc_parse_init_data_file();

    // Get BOINC task and app data
    // For more info on this structure see boinc/lib/app_ipc.h
    APP_INIT_DATA dataBOINC;

    boinc_get_init_data( dataBOINC );

    config.app_version = std::to_string( dataBOINC.app_version );
    config.app_name = dataBOINC.app_name;
    config.project_dir = dataBOINC.project_dir;
    config.project_dir += "/";    // Add trailing slash for consistent path handling
    config.boinc_dir = dataBOINC.boinc_dir;
    config.boinc_dir += "/";
    config.wu_name = dataBOINC.wu_name;
    config.result_name = dataBOINC.result_name;
    // convert ncpus from double to int.
    if ( dataBOINC.ncpus < 1.0 ) {
        config.ncpus = 1;    // only gpu tasks may have ncpus < 1.0 which CPDN do not use.
    } else {
        config.ncpus = int( dataBOINC.ncpus );
    }

    // Re-parse app version to add a dot
    // This assumes version is X.Y, X.YY or XX.YY format, will get it wrong if not.
    if ( auto vlen = config.app_version.length(); vlen == 2 ) {
        config.app_version.insert( 1, "." );
    } else if ( vlen > 2 ) {
        config.app_version.insert( vlen - 2, "." );
    }

    // Check whether BOINC is running in standalone mode
    config.standalone = boinc_is_standalone() == 1;

    // Set the task related paths.
    // The APP_INIT_DATA structure only has the slot number.
    config.slot_path = fs::current_path().string();

    // Set BOINC optional values
    BOINC_OPTIONS options;
    boinc_options_defaults( options );
    options.main_program = true;    // tell boinc client this is the main program
    // Nov/2025. This option appears to sometimes cause a memory corruption (!prev); possible race condition
    //          on 'static std::vector<UPLOAD_FILE_STATUS> upload_file_status'; in boinc_api.cpp upon destruction.
    //          Disable for now. Monitor code did not use it.
    //          If this is set true, the client will kill all our child processes on exit (supposedly).
    //          It also appears to execute suspend/resume on child processes, which we do here.
    //          If I disable this, then we should make sure all descendants are killed on exit.
    // Dec/2025. Testing shows that even with this option commented out we still get the memory corruption.
    //           TODO: try explicity setting the option to false?  Need to run a address sanitizer build as a batch to track this down.
    //options.multi_process = true;           // if your app uses multiple processes, do this before creating any threads or processes, or storing the PID
    options.check_heartbeat = true;           // controller monitors 'heartbeat' messages from the client.
    options.handle_process_control = true;    // controller will handle all suspend/quit/resume messages from the boinc client.
    options.direct_process_action = false;    // controller will respond to quit messages and heartbeat failures by exiting,
                                              // and will respond to suspend and resume messages by suspending and resuming
    options.send_status_msgs = false;         // If set, the program will report its CPU time and fraction done to the client.
                                              // Set in worker programs.

    return boinc_init_options( &options );
}


/**
 * @brief Copies the application zip file to the slot directory and unzips it.
 * 
 * @param app_name     The name of the application.
 * @param version      The version string of the application.
 * @param project_path The path to the project directory.
 * @param slot_path    The path to the slot directory.
 * @return int         Returns 0 on success, non-zero on failure.
 */
int move_and_unzip_app_file( const std::string& app_name, const std::string& version, const std::string& project_path, const std::string& slot_path )
{
    // GC. TODO. This code could be combined with copy_and_unzip() to avoid code duplication.

    int retval = 0;
    std::string app_file = app_name + "_app_" + version + "_" + CPDN_PLATFORM + ".zip";

    // Copy the app file to the working directory
    fs::path app_source = project_path;
    app_source /= app_file;
    fs::path app_destination = slot_path;
    app_destination /= app_file;
    std::cerr << "Copying: " << app_source << "\n     to: " << app_destination << "\n";

    // GC. Replace boinc copy with modern C++17 filesystem copy.  Overwrite to match boinc_copy behaviour.
    try {
        fs::copy_file( app_source, app_destination, fs::copy_options::overwrite_existing );
    } catch ( const fs::filesystem_error& e ) {
        std::cerr << "..move_and_unzip_app: Error copying file: " << app_source << " to: " << app_destination << ",\nError: " << e.what()
                  << std::endl;
        return 1;
    }

    // Unzip the app zipfile
    std::cerr << "Extracting the app zipfile: " << app_destination << "\n";

    if ( !cpdn_unzip( app_destination, slot_path ) ) {
        retval = 1;
        std::cerr << "..Extracting the app zipfile failed" << "\n";
        return retval;
    } else {
        try {
            fs::remove( app_destination );
        } catch ( const fs::filesystem_error& e ) {
            std::cerr << "..move_and_unzip_app_file(). Error removing file: " << app_destination << ",\nError: " << e.what() << std::endl;
        }
    }
    return retval;
}


/**
 * @brief Checks the status of a child process.
 * 
 * @param child_process The child process handle.
 * @param child_status The current status of the child process.
 * @param exit_code The exit code of the child process (set on normal exit).
 * @return The updated child status, unchanged if still running.
 */
int check_child_status( ChildProcessHandle& child_process, int child_status, int& exit_code )
{
    std::string err_msg;
    ChildProcessState process_state = poll_child_process( child_process, exit_code, err_msg );
    int updated_status = child_status_from_process_state( process_state, child_status );
    if ( updated_status == child_status ) {
        return updated_status;
    }

    if ( updated_status == 1 ) {
        std::cerr << "..The child process terminated with status: " << exit_code << '\n';
    } else if ( updated_status == 3 ) {
        std::cerr << "..The child process terminated abnormally or was forcibly ended" << '\n';
    } else if ( updated_status == 4 ) {
        std::cerr << "..The child process has been suspended" << '\n';
    } else if ( updated_status == 5 ) {
        exit_code = -1;
        std::cerr << "..Unable to retrieve status of child process";
        if ( !err_msg.empty() ) {
            std::cerr << ": " << err_msg;
        }
        std::cerr << '\n';
    }
    return updated_status;
}


/**
 * @brief Applies the latest BOINC client status to the child process.
 * 
 * @param child_process The child process handle.
 * @param runtime Holds the latest BOINC runtime status snapshot.
 * @return True if the controller should continue running, false on quit/abort/no-heartbeat.
 */
bool handle_boinc_client_status( ChildProcessHandle& child_process, BoincRuntime& runtime )
{
    std::string err_msg;

    // If a quit, abort or no heartbeat has been received from the BOINC client, end child process
    if ( runtime.client_status.quit_request ) {
        std::cerr << "Quit request received from BOINC client, ending the child process" << '\n';
        if ( !terminate_child_process( child_process, err_msg ) ) {
            std::cerr << "..Failed to terminate child process: " << err_msg << '\n';
        }
        return false;
    } else if ( runtime.client_status.abort_request ) {
        std::cerr << "Abort request received from BOINC client, ending the child process" << '\n';
        if ( !terminate_child_process( child_process, err_msg ) ) {
            std::cerr << "..Failed to terminate child process: " << err_msg << '\n';
        }
        return false;
    } else if ( runtime.client_status.no_heartbeat ) {
        std::cerr << "No heartbeat received from BOINC client, ending the child process" << '\n';
        if ( !terminate_child_process( child_process, err_msg ) ) {
            std::cerr << "..Failed to terminate child process: " << err_msg << '\n';
        }
        return false;
    }
    // Else if BOINC client is suspended, suspend child process and periodically refresh BOINC client status
    else {
        if ( runtime.client_status.suspended ) {
            std::cerr << "Suspend request received from the BOINC client, suspending the child process" << '\n';
            if ( !suspend_child_process( child_process, err_msg ) ) {
                std::cerr << "..Failed to suspend child process: " << err_msg << '\n';
                return false;
            }

            while ( runtime.client_status.suspended ) {
                boinc_get_status( &runtime.client_status );
                if ( runtime.client_status.quit_request ) {
                    std::cerr << "Quit request received from the BOINC client, ending the child process" << '\n';
                    if ( !terminate_child_process( child_process, err_msg ) ) {
                        std::cerr << "..Failed to terminate child process: " << err_msg << '\n';
                    }
                    return false;
                } else if ( runtime.client_status.abort_request ) {
                    std::cerr << "Abort request received from the BOINC client, ending the child process" << '\n';
                    if ( !terminate_child_process( child_process, err_msg ) ) {
                        std::cerr << "..Failed to terminate child process: " << err_msg << '\n';
                    }
                    return false;
                } else if ( runtime.client_status.no_heartbeat ) {
                    std::cerr << "No heartbeat received from the BOINC client, ending the child process" << '\n';
                    if ( !terminate_child_process( child_process, err_msg ) ) {
                        std::cerr << "..Failed to terminate child process: " << err_msg << '\n';
                    }
                    return false;
                }
                sleep_seconds( 1 );
            }
            // Resume child process
            std::cerr << "Resuming the child process" << "\n";
            if ( !resume_child_process( child_process, err_msg ) ) {
                std::cerr << "..Failed to resume child process: " << err_msg << '\n';
                return false;
            }
        }
        return true;
    }
}


/**
 * @brief Launches a child process to run the model executable.
 * 
 * @param project_path The path to the project directory.
 * @param slot_path The path to the slot directory.
 * @param strCmd The command to execute (model executable).
 * @param nthreads The number of threads to use.
 * @return ChildProcessHandle The launched child process handle, or an invalid handle on failure.
 */
ChildProcessHandle launch_process( const ModelControl& model_ctrl, const std::string& project_path, const std::string& slot_path,
                                   const std::string& strCmd, const std::string& nthreads )
{
    (void)project_path;

    std::string err_msg;
    ChildEnvironment env_vars = model_ctrl.get_env_vars( slot_path, nthreads, err_msg );
    if ( !err_msg.empty() ) {
        std::cerr << "..Failed to prepare child environment: " << err_msg << '\n';
        return {};
    }

    auto child_process = start_child_process( strCmd, slot_path, env_vars, err_msg );
    if ( !child_process_is_valid( child_process ) ) {
        std::cerr << "..Unable to start a new child process";
        if ( !err_msg.empty() ) {
            std::cerr << ": " << err_msg;
        }
        std::cerr << '\n';
        return {};
    }

    std::cerr << "The child process has been launched with process id: " << child_process.process_id << "\n";
    return child_process;
}


/**
 * @brief Resolves a BOINC logical input file to its physical path on disk.
 * 
 * @param logical_file The path to the logical BOINC file.
 * @param physical_path Output parameter to hold the resolved physical path.
 * @return true if the file was successfully resolved, false otherwise.
 */
bool resolve_boinc_input_file( const fs::path& logical_file, fs::path& physical_path, std::string* error_msg )
{
    if ( !fs::exists( logical_file ) ) {
        if ( error_msg ) {
            *error_msg = "logical BOINC file does not exist: " + logical_file.string();
        }
        return false;
    }

    std::string resolved = logical_file.string();
    int retval = boinc_resolve_filename_s( logical_file.string().c_str(), resolved );
    if ( retval ) {
        if ( error_msg ) {
            *error_msg = "boinc_resolve_filename_s() failed: " + std::string( boincerror( retval ) );
        }
        return false;
    }

    fs::path candidate = resolved;
#ifndef _WIN32
    if ( candidate == logical_file && fs::is_symlink( logical_file ) ) {
        candidate = fs::read_symlink( logical_file );
    }
#endif
    if ( candidate.is_relative() ) {
        candidate = logical_file.parent_path() / candidate;
    }
    candidate = candidate.lexically_normal();

    if ( !fs::exists( candidate ) ) {
        if ( error_msg ) {
            *error_msg = "resolved BOINC file does not exist: " + candidate.string();
        }
        return false;
    }

    physical_path = candidate;
    return true;
}


/**
 * @brief Verifies that the MD5 checksum of the project file matches the expected value extracted from the filename.
 * 
 * @param project_file The path to the project file.
 * @return true if the MD5 checksum matches the expected value, false otherwise.
 */
bool verify_project_zip_md5( const fs::path& project_file, std::string* error_msg )
{
    std::string expected_md5;
    if ( !extract_expected_md5( project_file, expected_md5 ) ) {
        if ( error_msg ) {
            *error_msg = "project file name is not of the form jf_<md5>: " + project_file.string();
        }
        return false;
    }

    char actual_md5[MD5_LEN] = { 0 };    // MD5_LEN comes from boinc include header.
    double nbytes = 0.0;
    int retval = md5_file( project_file.string().c_str(), actual_md5, nbytes );
    if ( retval ) {
        if ( error_msg ) {
            *error_msg = "failed to compute MD5: " + std::string( boincerror( retval ) );
        }
        return false;
    }

    std::string actual = lowercase_copy( actual_md5 );
    if ( actual != expected_md5 ) {
        if ( error_msg ) {
            *error_msg = "md5 mismatch: expected " + expected_md5 + ", got " + actual;
        }
        return false;
    }

    return true;
}


/**
 * @brief Stages a model input archive by copying from the project directory to the slot directory and unzipping.
 * 
 * @param source_project_file The path to the source archive file in the project directory.
 * @param slot_path The path to the slot directory.
 * @param unzip_relative_dir The relative directory within the slot to unzip the file to.
 * @param type_label A label for the type of file being staged, used in logging messages.
 * @return true if the file was successfully staged, false otherwise.
 */
InputStageResult stage_model_input_archive( const fs::path& source_project_file, const fs::path& slot_path, const fs::path& unzip_relative_dir,
                                            std::string_view type_label )
{
    fs::path unzip_dir = unzip_relative_dir.empty() || unzip_relative_dir == "." ? slot_path : ( slot_path / unzip_relative_dir ).lexically_normal();
    fs::path destination_archive = unzip_dir / source_project_file.filename();

    std::string error_msg;
    if ( !ensure_directory( unzip_dir, &error_msg ) ) {
        auto result = make_stage_error( "ensure_directory", std::move( error_msg ) );
        result.resolved_project_file = source_project_file;
        result.destination_archive = destination_archive;
        return result;
    }

    std::cerr << "Copying " << type_label << " from: " << source_project_file << "\n     to: " << destination_archive << '\n';
    try {
        fs::copy_file( source_project_file, destination_archive, fs::copy_options::overwrite_existing );
    } catch ( const fs::filesystem_error& e ) {
        auto result = make_stage_error( "copy_file", e.what() );
        result.resolved_project_file = source_project_file;
        result.destination_archive = destination_archive;
        return result;
    }

    std::cerr << "Unzipping " << type_label << " archive: " << destination_archive << '\n';
    if ( !cpdn_unzip( destination_archive, unzip_dir ) ) {
        auto result = make_stage_error( "cpdn_unzip", "failed to unzip archive into " + unzip_dir.string() );
        result.resolved_project_file = source_project_file;
        result.destination_archive = destination_archive;
        return result;
    }

    auto result = make_stage_success();
    result.resolved_project_file = source_project_file;
    result.destination_archive = destination_archive;
    return result;
}


/**
 * @brief Stages a BOINC input file by resolving the logical filename, verifying the MD5, copying to the slot directory and unzipping.
 * 
 * @param logical_file The logical filename of the BOINC input file.
 * @param slot_path The path to the slot directory.
 * @param unzip_relative_dir The relative directory within the slot to unzip the file to.
 * @param type_label A label for the type of file being staged, used in logging messages.
 * @return true if the file was successfully staged, false otherwise.
 */
InputStageResult stage_boinc_input_file( const fs::path& logical_file, const fs::path& slot_path, const fs::path& unzip_relative_dir,
                                         std::string_view type_label )
{
    fs::path project_file;
    std::string error_msg;
    if ( !resolve_boinc_input_file( logical_file, project_file, &error_msg ) ) {
        auto result = make_stage_error( "resolve_boinc_input_file", std::move( error_msg ) );
        result.logical_file = logical_file;
        return result;
    }

    if ( !verify_project_zip_md5( project_file, &error_msg ) ) {
        auto result = make_stage_error( "verify_project_zip_md5", std::move( error_msg ) );
        result.logical_file = logical_file;
        result.resolved_project_file = project_file;
        return result;
    }

    auto result = stage_model_input_archive( project_file, slot_path, unzip_relative_dir, type_label );
    result.logical_file = logical_file;
    if ( result.resolved_project_file.empty() ) {
        result.resolved_project_file = project_file;
    }
    return result;
}


/**
 * @brief Stages the model input files as specified in the manifest by copying from the 
 *        project directory to the slot directory and unzipping.
 * 
 * @param manifest The manifest specifying the model input files to stage.
 * @param slot_path The path to the slot directory.
 * @return true if all files were successfully staged, false otherwise.
 */
InputStageResult stage_model_input_manifest( const ModelInputManifest& manifest, const fs::path& slot_path )
{
    for ( const auto& archive : manifest ) {
        fs::path logical_file = slot_path / archive.logical_name;
        auto result = stage_boinc_input_file( logical_file, slot_path, archive.unzip_relative_dir, archive.logical_name );
        if ( !result.ok ) {
            return result;
        }
    }
    return make_stage_success();
}


/**
 * @brief Returns fraction completed of model run
 *        This is currently based on OpenIFS but should be general enough for other models.
 *        The function uses a 'heartbeat' mechanism to provide a smoother progress update
 *        between model steps.
 * @param step The current model step.
 * @param total_steps The total number of model steps.
 * @param nthreads The number of threads being used.
 * @return double The fraction completed (0.0 to 1.0).
 */
double model_frac_done( double step, double total_steps, int nthreads )
{
    static int stepm1 = -1;
    static double heartbeat = 0.0;
    static bool debug = false;

    double frac_done = step / total_steps;    // this increments slowly, as a model step is ~30sec->2mins cpu
    double frac_per_step = 1.0 / total_steps;

    if ( debug ) {
        std::cerr << "get_frac_done: step = " << step << '\n';
        std::cerr << "        total_steps = " << total_steps << '\n';
        std::cerr << "      frac_per_step = " << frac_per_step << '\n';
    }

    // Constant below represents estimate of how many times around the mainloop
    // before the model completes it's next step. This varies alot depending on model
    // resolution, computer speed, etc. Tune it looking at varied runtimes & resolutions!
    // Higher is better than lower to underestimate.
    //
    // Impact of speedup due to multiple threads is accounted for below.
    //
    // If we want more accuracy could use the ratio of the model timestep to 1h (T159 tstep) to
    // provide a 'slowdown' factor for higher resolutions.
    double heartbeat_inc = ( frac_per_step / ( 70.0 / static_cast<double>( nthreads ) ) );

    if ( (int)step > stepm1 ) {
        heartbeat = 0.0;
        stepm1 = (int)step;
    } else {
        heartbeat = heartbeat + heartbeat_inc;
        if ( heartbeat > frac_per_step )
            heartbeat = frac_per_step - 0.001;    // slightly less than the next step
        frac_done = frac_done + heartbeat;
    }

    if ( frac_done < 0.0 )
        frac_done = 0.0;
    if ( frac_done > 1.0 )
        frac_done = 0.9999;    // never 100% until wrapper finishes
    if ( debug ) {
        std::cerr << "    heartbeat_inc = " << heartbeat_inc << '\n';
        std::cerr << "    heartbeat     = " << heartbeat << '\n';
        double percent = frac_done * 100.0;
        std::cerr << "     percent done = " << percent << '\n';
    }

    return frac_done;
}


/**
 * @brief Moves the result file from the slot directory to the temporary project directory.
 */
int move_result_file( const fs::path& slot_path, const fs::path& temp_path, const std::string& result )
{
    int retval = 0;

    // Move result file to the temporary folder in the project directory
    fs::path result_file = slot_path / result;
    fs::path temp_file = temp_path / result;

    if ( fs::exists( result_file ) ) {
        std::cerr << "Moving result file: " << result_file.filename() << " to projects directory.\n";
        retval = boinc_copy( result_file.string().c_str(), temp_file.string().c_str() );

        // If result file has been successfully copied over, remove it from slots directory
        if ( !retval ) {
            try {
                fs::remove( result_file );
            } catch ( const fs::filesystem_error& e ) {
                std::cerr << "..move_result_file(). Error removing file: " << result_file << ", error: " << e.what() << "\n";
            }
        }
    }
    return retval;
}


/**
 * @brief Takes the zip file, checks existence and whether empty and copies it to destination and unzips it
 * @param zipfile The path to the zip file containing the jf_ reference.
 * @param destination The path to copy the zip file to.
 * @param unzip_path The path to unzip the contents to.
 * @param type A string indicating the type of file (for logging purposes).
 * @return int Returns 0 on success, non-zero on failure.
 */
// GC. TODO. Convert this to accept  fs::path args.
/**
 * @brief Zips the upload files and deletes the original files upon success.
 * 
 * @param upload_file The path to the output zip file.
 * @param zfl A vector of file paths to be zipped.
 * @return int Returns 0 on success, non-zero on failure.
 */
int zip_and_delete( const std::string& upload_file, const std::vector<std::filesystem::path>& zfl )
{

    // Time the compression for diagnostics
    auto start = chrono::high_resolution_clock::now();

    auto outcome = cpdn_zip( upload_file, zfl );
    int retval = outcome ? 0 : 1;

    auto stop = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>( stop - start );
    std::cerr << "Time taken to compress upload file: " << duration.count() << " ms\n";

    if ( retval ) {
        std::cerr << ".. compressing upload file failed" << std::endl;
        return retval;
    } else {
        // Files have been successfully zipped, they can now be deleted
        for ( const auto& file : zfl ) {
            // Delete the zipped file
            try {
                fs::remove( file );
            } catch ( const fs::filesystem_error& e ) {
                std::cerr << "Error deleting file: " << file << ", error: " << e.what() << '\n';
            }
        }
    }
    return retval;
}

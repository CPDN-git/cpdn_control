//
//  CPDN Controller Progress File Handler class implementation.
//
//     Glenn Carver, CPDN, 2026

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>    // testing only; to include std::cerr/cout
#include <string>
#include <string_view>
#include <system_error>

#include <unistd.h>    // for getpid()

#include "../lib/utils.h"
#include "../src/cpdn_control.h"    // for TaskState struct
#include "progressfile_handler.h"

namespace {

bool parse_double_value( const std::string& value, double& out, std::string& err_msg )
{
    const char* begin = value.c_str();
    char* end = nullptr;
    errno = 0;
    out = std::strtod( begin, &end );

    if ( begin == end ) {
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

}    // namespace


ProgressFileHandler::ProgressFileHandler( const std::string& slot_path )
{
    progressfile_path = fs::path( slot_path ) / fs::path( progressfile_name );
}


/**
 * @brief Write task progress from TaskState struct to progress file
 * @param task Reference to TaskState struct containing progress information
 * @param err_msg Error message set on failure
 * @return true on success, false on failure
 */
bool ProgressFileHandler::write( const TaskState& task, std::string& err_msg ) const
{
    err_msg.clear();
    fs::path tmp_path = progressfile_path;
    tmp_path += ".tmp";

    std::ofstream progress_file_out{ tmp_path, std::ios::out | std::ios::trunc };
    if ( !progress_file_out.is_open() ) {
        err_msg = "Failed to open temp progress file for writing: " + tmp_path.string();
        return false;
    }

    // Write out the new progress file. Truncates progress_file to zero bytes if it already exists (as in a model restart)
    // GC Oct/2025. Made progress file a fortran namelist, so models can read it to check the control process is still running.
    //              Also included controller pid so model has additional way to check if controller is still alive.
    progress_file_out << "! CPDN controller progress file & fortran namelist\n"
                      << "&CPDN\n"
                      << "control_pid=" << std::to_string( getpid() ) << '\n'
                      << "prior_acc_cpu_time=" << std::to_string( task.current_cpu_time ) << '\n'
                      << "upload_file_number=" << std::to_string( task.upload_file_number ) << '\n'
                      << "last_completed_step=" << std::to_string( task.last_completed_step ) << '\n'
                      << "last_upload_time=" << std::to_string( task.last_upload_time ) << '\n'
                      << "model_completed=" << std::to_string( task.model_completed ) << '\n'
                      << "/" << std::endl;

    if ( !progress_file_out ) {
        err_msg = "Failed to write temp progress file: " + tmp_path.string();
        progress_file_out.close();
        std::error_code rm_ec;
        fs::remove( tmp_path, rm_ec );    // call non-throwing remove (not interested in the error msg if it fails)
        return false;
    }

    progress_file_out.close();
    if ( !progress_file_out ) {
        err_msg = "Failed to close temp progress file: " + tmp_path.string();
        std::error_code rm_ec;
        fs::remove( tmp_path, rm_ec );
        return false;
    }

    std::error_code ec;
    fs::rename( tmp_path, progressfile_path, ec );
    if ( ec ) {
        err_msg = "Failed to replace progress file with updated file: " + progressfile_path.string() + " (" + ec.message() + ")";
        std::error_code rm_ec;
        fs::remove( tmp_path, rm_ec );
        return false;
    }

    return true;
}


/**
 * @brief Reads task progress from progress file into TaskState struct
 * @param task Reference to TaskState struct to populate
 * @param err_msg Returned error message on failure. Cleared on entry.
 * @return true on success, false on failure
 */
bool ProgressFileHandler::read( TaskState& task, std::string& err_msg ) const
{
    err_msg.clear();

    // Parse the progress_file
    std::string progress_line;
    std::ifstream progress_file_in{ progressfile_path };
    if ( !progress_file_in.is_open() ) {
        err_msg = "Failed to open progress file: " + progressfile_path.string();
        return false;
    }

    // Check progress file is not empty, use non-throwing version
    if ( std::error_code ec; fs::is_empty( progressfile_path, ec ) ) {
        err_msg = "Progress file is empty: " + progressfile_path.string();
        progress_file_in.close();
        return false;
    }

    bool saw_control_pid = false;
    bool saw_prior_acc_cpu_time = false;
    bool saw_upload_file_number = false;
    bool saw_last_completed_step = false;
    bool saw_last_upload_time = false;
    bool saw_model_completed = false;
    std::string key;
    std::string value;

    // Read the progress file
    while ( std::getline( progress_file_in, progress_line ) ) {

        trim_whitespace( progress_line );
        if ( progress_line.empty() ) {
            continue;
        }
        key.clear();
        value.clear();

        // Note. As of 2025, the progress file is formatted as a fortran namelist
        // so the model process can read it to check the controller process is ok.
        if ( !parse_namelist_key_value( progress_line, key, value ) ) {
            continue;
        }

        // We don't use control_pid in TaskState but read it to validate the file.
        if ( key == "control_pid" ) {
            int pid_value = 0;
            saw_control_pid = true;
            if ( !parse_int( value, pid_value, err_msg ) ) {
                err_msg = "control_pid: " + err_msg;
            }
        } else if ( key == "prior_acc_cpu_time" ) {
            saw_prior_acc_cpu_time = true;
            if ( !parse_double_value( value, task.prior_acc_cpu_time, err_msg ) ) {
                err_msg = "prior_acc_cpu_time: " + err_msg;
            }
        } else if ( key == "upload_file_number" ) {
            saw_upload_file_number = true;
            if ( !parse_int( value, task.upload_file_number, err_msg ) ) {
                err_msg = "upload_file_number: " + err_msg;
            }
        } else if ( key == "last_completed_step" || key == "last_step" ) {
            saw_last_completed_step = true;
            if ( !parse_int( value, task.last_completed_step, err_msg ) ) {
                err_msg = key + ": " + err_msg;
            }
        } else if ( key == "last_upload_time" || key == "last_upload" ) {
            saw_last_upload_time = true;
            if ( !parse_double_value( value, task.last_upload_time, err_msg ) ) {
                err_msg = key + ": " + err_msg;
            }
        } else if ( key == "model_completed" ) {
            saw_model_completed = true;
            if ( !parse_int( value, task.model_completed, err_msg ) ) {
                err_msg = "model_completed: " + err_msg;
            }
        }

        if ( !err_msg.empty() )    // error detected by parse_int
            break;
    }
    progress_file_in.close();

    if ( !err_msg.empty() ) {
        return false;
    }

    if ( !( saw_control_pid && saw_prior_acc_cpu_time && saw_upload_file_number && saw_last_completed_step && saw_last_upload_time &&
            saw_model_completed ) ) {
        err_msg = "Progress file missing required fields";
        return false;
    }

    return true;
}


/**
 * @brief Print the progress file to the provided output stream
 * @param os Output stream to use.
 */
void ProgressFileHandler::print( std::ostream& os ) const
{
    std::ifstream progress_file_in{ progressfile_path };
    std::string line;

    if ( !( progress_file_in.is_open() ) ) {
        progress_file_in.open( progressfile_path );
    }
    os << "\n --- Contents of file: " << progressfile_path.string() << " --- \n";
    while ( std::getline( progress_file_in, line ) ) {
        os << line << '\n';
    }
    progress_file_in.close();
}

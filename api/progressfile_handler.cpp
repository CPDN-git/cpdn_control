//
//  CPDN Controller Progress File Handler class implementation.
//
//     Glenn Carver, CPDN, 2026

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include <unistd.h>    // for getpid()

#include "../src/cpdn_control.h"    // for TaskState struct
#include "progressfile_handler.h"


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
                      << "last_cpu_time=" << std::to_string( task.last_cpu_time ) << '\n'
                      << "upload_file_number=" << std::to_string( task.upload_file_number ) << '\n'
                      << "last_step=" << task.last_step << '\n'
                      << "last_upload=" << std::to_string( task.last_upload ) << '\n'
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
 * @param progress_file Path to the progress file to read
 * @param task Reference to TaskState struct to populate
 */
void ProgressFileHandler::read( TaskState& task ) const
{
    // Parse the progress_file
    std::string progress_line = "";
    std::string delimiter = "=";
    std::ifstream progress_file_in{ progressfile_path };

    // Open the progress_file file
    if ( !( progress_file_in.is_open() ) ) {
        progress_file_in.open( progressfile_path );
    }

    // Read the namelist file
    while ( std::getline( progress_file_in, progress_line ) ) {    //get 1 row as a string

        if ( progress_line.find( "last_cpu_time" ) != std::string::npos ) {
            task.last_cpu_time = std::stoi( progress_line.substr( progress_line.find( delimiter ) + 1, progress_line.length() - 1 ) );
        } else if ( progress_line.find( "upload_file_number" ) != std::string::npos ) {
            task.upload_file_number = std::stoi( progress_line.substr( progress_line.find( delimiter ) + 1, progress_line.length() - 1 ) );
        } else if ( progress_line.find( "last_step" ) != std::string::npos ) {
            task.last_step = progress_line.substr( progress_line.find( delimiter ) + 1, progress_line.length() - 1 );
        } else if ( progress_line.find( "last_upload" ) != std::string::npos ) {
            task.last_upload = std::stoi( progress_line.substr( progress_line.find( delimiter ) + 1, progress_line.length() - 1 ) );
        } else if ( progress_line.find( "model_completed" ) != std::string::npos ) {
            task.model_completed = std::stoi( progress_line.substr( progress_line.find( delimiter ) + 1, progress_line.length() - 1 ) );
        }
    }
    progress_file_in.close();
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

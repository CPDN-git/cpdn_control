//
// Implementation of the OpenIFS model control class.
//  Glenn Carver, CPDN, 2025.

#include "oifs_control.h"
#include "../../lib/utils.h"
#include "oifs_utils.h"    // for oifs_parse_stat()
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

// Implementations of the pure virtual functions from ModelControl

/**
 * @brief Check if the model has completed successfully. Call this after model task has finished.
 * @returns True if the model completed successfully, false otherwise.
 */
bool OpenIFSControl::check_model_success() const
{
    bool success = false;

    // To check whether the model completed successfully, look for 'CNT0' in 3rd column of ifs.stat
    // This will always be the last line of a successful model forecast.

    if ( fs::exists( ifs_stat ) ) {
        std::string ifs_word = "";
        std::string stat_lastline = "";

        fread_last_line( ifs_stat.string(), stat_lastline );    // at some point, these will all be fs::path..
        oifs_parse_stat( stat_lastline, ifs_word, 3 );
        std::cerr << "Last line of ifs.stat, ifs_word: " << stat_lastline << ", " << ifs_word << '\n';
        if ( ifs_word == "CNT0" ) {
            success = true;
        } else {
            std::cerr << "CNT0 not found; string returned was: " << "'" << ifs_word << "'" << '\n';
        }
    } else {
        std::cerr << "ifs.stat file not found: " << ifs_stat << '\n';
    }
    return success;
}

/**
 * @brief Print the last n lines of key log files produced by the model.
 * @param nlines Number of lines to print from end of each log file.
 */
void OpenIFSControl::print_logs( const int nlines ) const
{
    // TODO: could this be pushed down to the base class rather than re-implemented in each derived class?
    for ( const auto& log_file : log_files ) {
        print_last_lines( log_file, nlines );    // from lib/utils.h; will check file exists
    }
}


/**
 * @brief Get the current model step from the status file.
 * @param status_file Path to the model status file.
 * @param current_step Reference to an integer to store the current step. Updated on success.
 * @returns True if the current step was successfully retrieved, false otherwise.
 */
bool OpenIFSControl::get_current_step( std::string& current_step, const int total_steps ) const
{
    bool result = false;
    std::string iter = "0";
    std::string lastline{};

    // Read completed step from last line of ifs.stat file.
    // Note the first line from the model has a step count of '....  CNT3      -999 ....'
    // When the iteration number changes in the ifs.stat file, OpenIFS has completed writing
    // to the output files for that iteration, those files can now be moved and uploaded.
    //std::cerr << "Reading completed iteration step from last line of ifs.stat" << std::endl;

    if ( fread_last_line( ifs_stat.string(), lastline ) ) {      // only returns true if lastline is read and changed.
        if ( oifs_parse_stat( lastline, current_step, 4 ) ) {    // iter updates
            if ( oifs_valid_step( iter, total_steps ) ) {
                result = true;
            }
        }
    }
    return result;
}

/**
 * @brief Returns list of model output filenames at a model step.
 *        Used to determine which files to upload at each step.
 * @param step The model step (string) of files to return.
 * @param id The experiment ID or general experiment identifier (string).
 * @returns A vector of output filenames to be uploaded.
 */
std::vector<std::string> OpenIFSControl::get_output_filenames( std::string_view step, std::string_view exptid ) const
{
    std::string suffix = oifs_get_filename_part( std::string( step ), std::string( exptid ) );
    std::cerr << "get_output_filename: exptid should come from the model instance, not via the args\n";
    return { "ICMGG" + suffix, "ICMSH" + suffix, "ICMUA" + suffix };
}


/**
 * @brief Returns regex of the OpenIFS GRIB model output filename pattern
 */
std::regex OpenIFSControl::get_output_filename_regex() const { return output_file_pattern; }


/**
 * @brief Returns vector of list of log files.
 */
std::vector<std::string> OpenIFSControl::get_log_filenames() const { return log_files; }


/** 
 * @brief Returns true if OpenIFS rcf file exists in current dir 
 */
bool OpenIFSControl::restart_ctl_exists() const { return fs::exists( rcf ); }


/**
 * @brief Reads the OpenIFs restart control namelist file "rcf"
 */
bool OpenIFSControl::restart_ctl_read( std::string& step, std::string& time ) const
{
    std::ifstream rcf_stream;
    bool ok = false;

    if ( !fs::exists( rcf ) ) {
        return ok;
    }
    // Read the rcf file
    if ( !( rcf_stream.is_open() ) ) {
        rcf_stream.open( rcf );
    }
    if ( rcf_stream.is_open() ) {
        if ( oifs_read_rcf_file( rcf_stream, time, step ) ) {
            std::cerr << "Read the rcf file" << '\n';
            ok = true;
        }
    }
    rcf_stream.close();

    return ok;
}

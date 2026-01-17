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
 * @param ifsstat_path Path to the ifs.stat file.
 * @returns True if the model completed successfully, false otherwise.
 */
bool OpenIFSControl::check_model_success( std::string_view ifsstat_path ) const
{
    bool success = false;

    // To check whether the model completed successfully, look for 'CNT0' in 3rd column of ifs.stat
    // This will always be the last line of a successful model forecast.

    if ( path_exists( ifsstat_path ) ) {
        std::string ifs_word = "";
        std::string stat_lastline = "";

        fread_last_line( std::string( ifsstat_path ), stat_lastline );    // at some point, these will all be fs::path..
        oifs_parse_stat( stat_lastline, ifs_word, 3 );
        std::cerr << "Last line of ifs.stat, ifs_word: " << stat_lastline << ", " << ifs_word << '\n';
        if ( ifs_word == "CNT0" ) {
            success = true;
        } else {
            std::cerr << "CNT0 not found; string returned was: " << "'" << ifs_word << "'" << '\n';
        }
    } else {
        std::cerr << "ifs.stat file not found: " << ifsstat_path << '\n';
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
bool OpenIFSControl::get_current_step( const std::string& ifs_stat, std::string& current_step, const int total_steps ) const
{
    bool result = false;
    std::string iter = "0";
    std::string stat_lastline{};

    // Read completed step from last line of ifs.stat file.
    // Note the first line from the model has a step count of '....  CNT3      -999 ....'
    // When the iteration number changes in the ifs.stat file, OpenIFS has completed writing
    // to the output files for that iteration, those files can now be moved and uploaded.
    //std::cerr << "Reading completed iteration step from last line of ifs.stat" << std::endl;

    if ( fread_last_line( ifs_stat, stat_lastline ) ) {               // only returns true if lastline is read and changed.
        if ( oifs_parse_stat( stat_lastline, current_step, 4 ) ) {    // iter updates
            if ( oifs_valid_step( iter, total_steps ) ) {
                result = true;
            }
        }
    }
    return result;
}
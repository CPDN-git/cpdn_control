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

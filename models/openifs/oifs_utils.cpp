//
//  OpenIFS utility functions
//  To be called by Model classes for the OpenIFS based models.
//  Not to be called by the CPDN controller directly.
//
//    Glenn Carver, CPDN, 2025.


#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "../../lib/utils.h"
#include "oifs_utils.h"


/**
 *  @brief Construct the filename part of the output model filename containing the iteration count.
 *          nb. exptid is always 4 characters for OpenIFS.
 *  @param last_iter The last completed iteration as a string.
 *  @param exptid The experiment ID string.
 *  @return The constructed filename part string.
 */
std::string oifs_get_filename_part( const std::string& last_iter, const std::string& exptid )
{
    std::ostringstream oss;
    oss << exptid << "+" << std::setw( 6 ) << std::setfill( '0' ) << last_iter;
    return oss.str();
}


/**
 *  @brief Parse a line of the OpenIFS ifs.stat log file.
 *  @param logline  : incoming ifs.stat logfile line to be parsed
 *  @param stat_col : returned string given by position 'index'
 *  @return false if string is empty.
 */
bool oifs_parse_stat( const std::string& logline, std::string& stat_column, const int index )
{
    std::istringstream tokens;
    std::string statstr = "";

    //  split input, get token specified by 'column' unless file is corrupted
    tokens.str( logline );
    for ( int i = 0; i < index; ++i )
        tokens >> statstr;

    if ( statstr.empty() ) {
        std::cerr << "..oifs_parse_stat: warning, statstr is empty: " << logline << '\n';
        return false;
    } else {
        stat_column = statstr;
        return true;
    }
}


/**
 * @brief Check for a valid step count.
 * @param step The step count as a string.
 * @param nsteps The total number of steps in the model run.
 * @return true if step is valid, otherwise false
 */
bool oifs_valid_step( std::string& step, int nsteps )
{
    // make sure step is valid integer
    int istep;
    std::string err_msg;

    if ( !parse_int( step, istep, err_msg ) ) {
        std::cerr << "..oifs_valid_step: unable to convert step to int: " << step << ", error: " << err_msg << '\n';
        return false;
    } else {
        // check step is in valid range: 0 -> total no. of steps
        if ( istep < 0 ) {
            return false;
        } else if ( istep > nsteps ) {
            return false;
        }
    }
    return true;
}


/**
 * @brief Read the rcf_file line by line and extract CTIME and CSTEP variables.
 *        The input stream rcf_file must be at file start and ctime_value & cstep_value
 *        must be empty strings.
 */
bool oifs_read_rcf_file( std::ifstream& rcf_file, std::string& ctime_value, std::string& cstep_value )
{
    std::string rcf_file_line;
    std::string parsed_key;
    std::string parsed_value;

    // Extract the values of CSTEP and CTIME from the rcf file
    while ( std::getline( rcf_file, rcf_file_line ) ) {
        parsed_key.clear();
        parsed_value.clear();

        if ( !parse_namelist_key_value( rcf_file_line, parsed_key, parsed_value ) ) {
            continue;
        }

        if ( parsed_key == "CSTEP" ) {
            cstep_value = parsed_value;
        } else if ( parsed_key == "CTIME" ) {
            ctime_value = parsed_value;
        }
    }

    if ( cstep_value.empty() ) {
        std::cerr << "CSTEP value not present in rcf file" << '\n';
        return false;
    } else if ( ctime_value.empty() ) {
        std::cerr << "CTIME value not present in rcf file" << '\n';
        return false;
    } else {
        return true;
    }
}

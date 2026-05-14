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
 * @brief Return the OpenIFS GRIB environment variables derived from the slot directory.
 */
std::vector<std::pair<std::string, std::string>> oifs_get_grib_env_vars( const std::string& slot_path )
{
    std::vector<std::pair<std::string, std::string>> env_vars;
    env_vars.emplace_back( "GRIB_SAMPLES_PATH", slot_path + "/eccodes/ifs_samples/grib1_mlgrib2" );
    env_vars.emplace_back( "GRIB_DEFINITION_PATH", slot_path + "/eccodes/definitions" );
    return env_vars;
}


/**
 * @brief Return the OpenMP environment variables to use for model and associated processes.
 */
std::vector<std::pair<std::string, std::string>> oifs_get_omp_env_vars( const std::string& nthreads )
{
    std::vector<std::pair<std::string, std::string>> env_vars;
    env_vars.emplace_back( "OMP_NUM_THREADS", nthreads );
    env_vars.emplace_back( "OMP_SCHEDULE", "STATIC" );    // Enforce static scheduling for OpenMP threads.
    env_vars.emplace_back( "OMP_STACKSIZE", "128M" );     // OpenIFS needs more stack per thread
    return env_vars;
}

/**
 * @brief Build the required OpenIFS child-process environment variables.
 *        OMP_NUM_THREADS uses the incoming nthreads value; other values are fixed.
 */
bool oifs_get_model_env_vars( const std::string& slot_path, const std::string& nthreads, std::vector<std::pair<std::string, std::string>>& env_vars,
                              std::string& err_msg )
{
    err_msg.clear();
    env_vars.clear();

    // check nthreads is valid integer
    // parse_int can modify the string!
    if ( std::string nthreads_copy = nthreads; !parse_int( nthreads_copy ) ) {
        err_msg = "invalid value of 'nthreads': " + nthreads;
        return false;
    }

    // OIFS_DUMMY_ACTION controls what the model does when it gets into a dummy subroutine.
    // Possible values are 'quiet', 'verbose' or 'abort'. We use 'abort' to stop the model.
    env_vars.emplace_back( "OIFS_DUMMY_ACTION", "abort" );
    env_vars.emplace_back( "DR_HOOK", "1" );                // Enable DrHook tracing; 1=on, 0=off.
    env_vars.emplace_back( "DR_HOOK_HEAPCHECK", "no" );     // Report heap size stats at end; yes/no.
    env_vars.emplace_back( "DR_HOOK_STACKCHECK", "no" );    // Report stack usage stats at end; yes/no.
    env_vars.emplace_back( "EC_MEMINFO", "0" );             // disable noisy EC_MEMINFO output
    env_vars.emplace_back( "EC_PROFILE_HEAP", "0" );        // disable heap stats; does not work with CPDN version.
    env_vars.emplace_back( "EC_PROFILE_MEM", "0" );         // disable memory stats; does not work with CPDN version.

    auto grib_env_vars = oifs_get_grib_env_vars( slot_path );
    env_vars.insert( env_vars.end(), grib_env_vars.begin(), grib_env_vars.end() );

    auto omp_env_vars = oifs_get_omp_env_vars( nthreads );
    env_vars.insert( env_vars.end(), omp_env_vars.begin(), omp_env_vars.end() );

    return true;
}


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

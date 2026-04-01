//
//  BOINC TrickleHandler class implementation for CPDN
//
//     Glenn Carver, CPDN, 2025.

#include "trickle_handler.h"
#include "boinc/boinc_api.h"
#include <cctype>
#include <chrono>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>


TrickleHandler::TrickleHandler( const std::string& wu, const std::string& result_base, const std::string& slot )
    : wu_name( wu ), result_base_name( result_base ), slot_path( slot )
{
    // Initialize the variety buffer once during construction.
    // Since variety is const, this buffer never changes after initialization.
    m_variety_buffer.insert( m_variety_buffer.end(), variety.begin(), variety.end() );
    m_variety_buffer.push_back( '\0' );
}


/**
 * @brief Read and sanitize trickle data from 'trickle_data' file in current working directory.
 * 
 * File format: comma-separated integers. The function:
 * - Returns empty string if file doesn't exist (silent).
 * - Warns if file exists but is empty.
 * - Removes trailing comma (if present).
 * - Sanitizes to keep only digits, commas, and minus signs; warns if chars were stripped.
 * - Warns and truncates if content exceeds 509 characters (max = 510 with null terminator).
 * 
 * @return Sanitized trickle data string, or empty string if file not found.
 */
std::string TrickleHandler::read_trickle_data_file() const
{
    std::ifstream trickle_file( std::string( TRICKLE_DATA_FILE ), std::ios::binary );

    // File doesn't exist - silently return empty string
    if ( !trickle_file.is_open() ) {
        return "";
    }

    // Read entire file content
    std::string raw_data( ( std::istreambuf_iterator<char>( trickle_file ) ), std::istreambuf_iterator<char>() );
    trickle_file.close();

    // Check for empty file
    if ( raw_data.empty() ) {
        std::cerr << "Warning: trickle_data file exists but is empty\n";
        return "";
    }

    // Remove trailing comma if present
    if ( !raw_data.empty() && raw_data.back() == ',' ) {
        raw_data.pop_back();
    }

    // Sanitize: keep only digits, commas, decimal points and minus signs
    // white space is removed to avoid excess length.
    std::string sanitized;
    bool had_invalid = false;

    for ( char c : raw_data ) {
        if ( isdigit( c ) || c == ',' || c == '-' || c == '.' ) {
            sanitized += c;
        } else {
            had_invalid = true;
        }
    }

    // Warn if invalid characters were found and removed
    if ( had_invalid ) {
        std::cerr << "Warning: trickle_data file contained invalid characters; stripped whitespace and any invalid chars\n";
    }

    // Check size limit (511 chars + 1 null terminator = 512 max)
    if ( sanitized.length() > MAX_DATA_LEN ) {
        std::cerr << "Warning: trickle_data content exceeds " << MAX_DATA_LEN << " characters; truncating\n";
        sanitized = sanitized.substr( 0, MAX_DATA_LEN );
    }

    return sanitized;
}


/**
 * @brief Construct and upload a trickle message to the CPDN server.
 * @param current_cpu_time The current CPU time used by the task.
 * @param timestep The current timestep of the model.
 */
int TrickleHandler::process_trickle( double current_cpu_time, int timestep )
{
    std::string ph = "";
    std::string vr = "";
    std::string data = read_trickle_data_file();
    std::string trickle_msg;
    const int trickle_cpu_time = static_cast<int>( current_cpu_time );
    int retval = 0;

    if ( variety == "orig" ) {
        // cpdn_credit parses <cp> with BOINC's integer XML parser, so emit an
        // integer CPU time even though the controller tracks it as a double.
        trickle_msg = fmt::format( ORIG_TRICKLE_FORMAT, wu_name, result_base_name, ph, timestep, trickle_cpu_time, vr );
    } else if ( variety == "general" ) {
        trickle_msg = fmt::format( GENERAL_TRICKLE_FORMAT, wu_name, result_base_name, ph, timestep, trickle_cpu_time, vr, data );
    } else {
        std::cerr << "Error: Unrecognized trickle variety: " << variety << "\n";
        return -1;
    }

    // Populate the trickle_msg buffer with null-terminated data.
    // This buffer is dynamic and changes with each trickle call.
    // The variety_buffer is pre-initialized in the constructor and reused.
    m_trickle_buffer.clear();
    m_trickle_buffer.insert( m_trickle_buffer.end(), trickle_msg.begin(), trickle_msg.end() );
    m_trickle_buffer.push_back( '\0' );

    std::cerr << "Sending trickle message to CPDN at timestep: " << timestep << "\n";
    retval = boinc_send_trickle_up( m_variety_buffer.data(), m_trickle_buffer.data() );

    // Diagnostic delay to test if async BOINC thread processing is the source of race conditions.
    // If double-free errors disappear with a delay here, it confirms async access by BOINC threads.
    // NOTE: This delay is temporary for diagnostic purposes and should be removed once the
    // race condition hypothesis is confirmed or ruled out.
    std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );

    if ( retval != 0 ) {
        std::cerr << "Error sending trickle, boinc_send_trickle_up returned: " << retval << "\n";
    }
    return retval;
}


/**
 * @brief Calculate the trickle frequency based on timestep (secs) and total number of model timesteps.
 *        Returns a trickle frequency of 10% of model run, with a minimum of every 24 model hours.
 * @param timestep     The model timestep in seconds.
 * @param total_nsteps The total number of steps in the model run.
 * @return The trickle frequency in model steps.
 */
int TrickleHandler::get_trickle_frequency( int timestep, int total_timesteps )
{
    //GC. Oct/25. Trickles are now at a fixed frequnency and not changeable by the user.

    int freq_min = ( 24 * 3600 ) / timestep;    // minimum of a trickle every 24 model hrs.
    int trickle_percent = 6;                    // trickle every 6% of the model run, i.e. 17 trickles in total.

    double freq = static_cast<double>( total_timesteps ) * static_cast<double>( trickle_percent ) / 100.0;
    auto trickle_freq = static_cast<int>( freq );
    if ( trickle_freq < freq_min ) {
        trickle_freq = freq_min;
    }
    return trickle_freq;
}

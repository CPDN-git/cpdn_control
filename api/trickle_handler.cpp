//
//  BOINC TrickleHandler class implementation for CPDN
//
//     Glenn Carver, CPDN, 2025.

#include "trickle_handler.h"
#include "boinc/boinc_api.h"
#include <fmt/format.h>
#include <iostream>
#include <vector>


TrickleHandler::TrickleHandler( const std::string& wu, const std::string& result_base, const std::string& slot )
    : wu_name( wu ), result_base_name( result_base ), slot_path( slot )
{
}


/**
 * @brief Construct and upload a trickle message to the CPDN server.
 * @param current_cpu_time The current CPU time used by the task.
 * @param timestep The current timestep of the model.
 */
int TrickleHandler::process_trickle( double current_cpu_time, int timestep ) const
{
    std::string ph = "";
    std::string vr = "";
    std::string data = "";
    std::string trickle_msg;
    int retval = 0;

    if ( variety == "orig" ) {
        trickle_msg = fmt::format( ORIG_TRICKLE_FORMAT, wu_name, result_base_name, ph, timestep, current_cpu_time, vr );
    } else if ( variety == "general" ) {
        trickle_msg = fmt::format( GENERAL_TRICKLE_FORMAT, wu_name, result_base_name, ph, timestep, current_cpu_time, vr, data );
    } else {
        std::cerr << "Error: Unrecognized trickle variety: " << variety << "\n";
        return -1;
    }

    // Create null terminated, non-const char buffers for the boinc_send_trickle_up call
    // to avoid possible memory faults (as seen in the past).
    std::vector<char> variety_data( variety.begin(), variety.end() );
    variety_data.push_back( '\0' );

    std::vector<char> trickle_data( trickle_msg.begin(), trickle_msg.end() );
    trickle_data.push_back( '\0' );

    std::cerr << "Sending trickle message to CPDN at timestep: " << timestep << "\n";
    retval = boinc_send_trickle_up( variety_data.data(), trickle_data.data() );
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
    //GC. Oct/25. Trickles are now fixed at every 10% of the model run with a final trickle at the end of the run.

    int freq_min = ( 24 * 3600 ) / timestep;    // minimum of a trickle every 24 model hrs.
    int fraction = 10;

    int trickle_freq = int( total_timesteps ) / fraction;
    if ( trickle_freq < freq_min ) {
        trickle_freq = freq_min;
    }
    return trickle_freq;
}

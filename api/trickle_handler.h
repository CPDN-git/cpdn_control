//
//  BOINC TrickleHandler class header for CPDN controller.
//
//     Glenn Carver, CPDN, 2025.

#pragma once

#include <string>

// This is the 'variety' or 'type' of the trickle message. It's used on the server side
// to determine how to process the trickle contents. For OpenIFS, we use the 'general'
// trickle variety which includes a 'data' field allowing us to send a small amount of
// model output or diagnostics to the Oxford server for batch analysis.
// The other variety is 'orig' which is used by the Hadley models and does not have the
// 'data' field.

const std::string variety = "general";

class TrickleHandler {

  public:
    TrickleHandler( const std::string& wu_name, const std::string& result_base_name, const std::string& slot_path );

    ~TrickleHandler() = default;

    // make static so we can call this outside of a class.
    static int get_trickle_frequency( int timestep, int total_timesteps );


    // Delete copy constructor and assignment operator
    // as these are not appropriate for this class.

    TrickleHandler( const TrickleHandler& ) = delete;
    TrickleHandler& operator=( const TrickleHandler& ) = delete;

    // Delete move constructor and assignment operator

    TrickleHandler( TrickleHandler&& ) = delete;
    TrickleHandler& operator=( TrickleHandler&& ) = delete;

    // Construct and upload a trickle message
    void process_trickle( double current_cpu_time, int timestep ) const;

  private:
    std::string wu_name;
    std::string result_base_name;
    std::string slot_path;
};
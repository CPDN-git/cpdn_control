//
//  BOINC TrickleHandler class header for CPDN
//
//     Glenn Carver, CPDN, 2025.

#pragma once

#include <string>

class TrickleHandler {

  public:
    TrickleHandler( const std::string& wu_name, const std::string& result_base_name, const std::string& slot_path );

    ~TrickleHandler();    // Destructor

    static int get_trickle_frequency( int timestep,
                                      int total_timesteps );    // static as does not depend on a class instance.


    // Delete copy constructor and assignment operator
    // as these are not appropriate for this class.
    TrickleHandler( const TrickleHandler& ) = delete;
    TrickleHandler& operator=( const TrickleHandler& ) = delete;

    // Delete move constructor and assignment operator
    TrickleHandler( TrickleHandler&& ) = delete;
    TrickleHandler& operator=( TrickleHandler&& ) = delete;

    // Construct and upload a trickle message
    void process_trickle( double current_cpu_time, int timestep );

  private:
    std::string wu_name_;
    std::string result_base_name_;
    std::string slot_path_;
};
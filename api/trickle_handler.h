//
//  BOINC TrickleHandler class header for CPDN controller.
//
//     Glenn Carver, CPDN, 2025.

#pragma once

#include <string>
#include <vector>

// Forward declare test function as friend to allow access to private members
int t_trickle_handler();

class TrickleHandler {

    friend int t_trickle_handler();    // Allow unit test in 'test/unit/' to access private members

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
    int process_trickle( double current_cpu_time, int timestep );

  private:
    // Read and sanitize trickle data from 'trickle_data' file in current working directory.
    std::string read_trickle_data_file() const;

    std::string wu_name;
    std::string result_base_name;
    std::string slot_path;

    // This is the 'variety' or 'type' of the trickle message. It's used on the server side
    // to determine how to process the trickle contents. For OpenIFS, we use the 'general'
    // trickle variety which includes a 'data' field allowing us to send a small amount of
    // model output or diagnostics to the Oxford server for batch analysis.
    // The other variety is 'orig' which is used by the Hadley models and does not have the
    // 'data' field.

    const std::string_view variety = "general";    // in time this could be a class init parameter.

    // Trickle formats currently recognized by CPDN server.

    std::string_view ORIG_TRICKLE_FORMAT = "<wu>{}</wu>\n<result>{}</result>\n<ph>{}</ph>\n<ts>{}</ts>\n<cp>{}</cp>\n<vr>{}</vr>\n";
    std::string_view GENERAL_TRICKLE_FORMAT =
        "<wu>{}</wu>\n<result>{}</result>\n<ph>{}</ph>\n<ts>{}</ts>\n<cp>{}</cp>\n<vr>{}</vr>\n<data>{}</data>\n";

    // Persistent buffers for trickle data passed to BOINC.
    // These are member variables to ensure extended lifetime, as BOINC may process
    // trickle data asynchronously via background threads. Storing pointers to
    // temporary stack buffers could result in use-after-free race conditions.
    std::vector<char> m_variety_buffer;
    std::vector<char> m_trickle_buffer;
};

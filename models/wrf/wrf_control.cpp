//
// Implementation of the WRF 4.6.1 Urban model control class.
//   Glenn Carver, CPDN, 2026.

#include "wrf_control.h"

#include "../../lib/utils.h"
#include <array>
#include <cctype>
#include <fstream>
#include <iostream>
#include <string>


// Helpers
namespace {

// These are fixed-size compile-time lookup tables, so std::array is the right tool:
// it keeps the size in the type and avoids the dynamic allocation/extra machinery that std::vector would add.
constexpr std::array<std::string_view, 3> WRF_OUTPUT_PREFIXES = { "wrfout_d01_", "wrfout_d02_", "wrfout_d03_" };
constexpr std::array<std::string_view, 3> WRF_RESTART_PREFIXES = { "wrfrst_d01_", "wrfrst_d02_", "wrfrst_d03_" };

std::vector<std::string> wrf_get_omp_env_vars( const std::string& nthreads ) { return { "OMP_NUM_THREADS=" + nthreads }; }


bool is_wrf_datetime_suffix( std::string_view suffix )
{
    if ( suffix.size() != 19 ) {
        return false;
    }

    constexpr std::array<int, 14> digit_positions = { 0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18 };
    for ( int pos : digit_positions ) {
        if ( !is_ascii_digit( suffix[static_cast<std::size_t>( pos )] ) ) {
            return false;
        }
    }

    return suffix[4] == '-' && suffix[7] == '-' && suffix[10] == '_' && suffix[13] == ':' && suffix[16] == ':';
}

bool is_wrf_prefixed_datetime_filename( std::string_view filename, std::string_view prefix )
{
    return filename.size() == prefix.size() + 19 && filename.rfind( prefix, 0 ) == 0 && is_wrf_datetime_suffix( filename.substr( prefix.size() ) );
}

bool extract_namelist_rhs_preserving_list( const std::string& line, std::string& value )
{
    std::string working_line = line;
    if ( auto comment_pos = working_line.find( '!' ); comment_pos != std::string::npos ) {
        working_line = working_line.substr( 0, comment_pos );
    }

    auto equals_pos = working_line.find( '=' );
    if ( equals_pos == std::string::npos ) {
        return false;
    }

    value = working_line.substr( equals_pos + 1 );
    trim_whitespace( value );
    if ( !value.empty() && value.back() == ',' ) {
        value.pop_back();
        trim_whitespace( value );
    }
    return !value.empty();
}

bool parse_wrf_domain_list_value( const std::string& values, int domain_index, int& parsed_value, std::string& err_msg )
{
    err_msg.clear();
    if ( domain_index <= 0 ) {
        err_msg = "domain index must be positive";
        return false;
    }

    std::size_t start = 0;
    int current_index = 1;
    while ( start <= values.size() ) {
        std::size_t end = values.find( ',', start );
        std::string token = values.substr( start, end == std::string::npos ? std::string::npos : end - start );
        trim_whitespace( token );

        if ( current_index == domain_index ) {
            if ( token.empty() ) {
                err_msg = "empty value for requested domain";
                return false;
            }
            return parse_int( token, parsed_value, err_msg );
        }

        if ( end == std::string::npos ) {
            break;
        }
        start = end + 1;
        ++current_index;
    }

    err_msg = "requested domain value not present";
    return false;
}

template <std::size_t N> bool matches_any_wrf_prefix( std::string_view filename, const std::array<std::string_view, N>& prefixes )
{
    for ( std::string_view prefix : prefixes ) {
        if ( is_wrf_prefixed_datetime_filename( filename, prefix ) ) {
            return true;
        }
    }
    return false;
}

}    // namespace


// Implementations of the virtual functions from ModelControl

/**
 * @brief Check if the model has succeeded. Must be called after model process terminates.
 * @returns True if successful, otherwise false.
 */
bool WRFControl::check_model_success() const
{
    bool success = false;
    fs::path stderr_out = "stderr.txt";

    // Check for 'success' string in WRF output.
    // Output is normally written to stdout when running standalone but the control
    // process will merge stdout onto stderr and the boinc init call redirects stderr
    // to stderr.txt. So, check the final line in the stderr.txt file.

    if ( fs::exists( stderr_out ) ) {
        std::string last_line{};

        fread_last_line( stderr_out.string(), last_line );
        if ( last_line.find( "SUCCESS COMPLETE WRF" ) != std::string::npos ) {
            success = true;
            std::cerr << "'SUCCESS COMPLETE WRF' found in model log. Model succeeded." << '\n';
        } else {
            std::cerr << "Did not find 'SUCCESS COMPLETE WRF' in model log. Model failed." << '\n';
        }
    } else {
        std::cerr << "Warning! Could not find model log : " << stderr_out << '\n';
    }

    return success;
}


/**
 * @brief WRF does not have any additional log files other than stdout & stderr.
 */
void WRFControl::print_logs( const int nlines ) const { (void)nlines; }


ModelInputManifest WRFControl::get_input_manifest( const std::string& wu ) const
{
    (void)wu;
    return {};
}

std::vector<std::string> WRFControl::get_env_vars( const std::string& slot_path, const std::string& nthreads, std::string& err_msg ) const
{
    (void)slot_path;
    err_msg.clear();

    if ( std::string nthreads_copy = nthreads; !parse_int( nthreads_copy ) ) {
        err_msg = "invalid value of 'nthreads': " + nthreads;
        return {};
    }

    return wrf_get_omp_env_vars( nthreads );
}


bool WRFControl::get_current_step( int& step, const int total_steps ) const
{
    (void)total_steps;
    step = 0;
    return false;
}


std::vector<std::string> WRFControl::get_output_filenames( int step ) const
{
    (void)step;
    return {};
}


bool WRFControl::is_output_filename( std::string_view filename ) const { return matches_any_wrf_prefix( filename, WRF_OUTPUT_PREFIXES ); }


bool WRFControl::is_restart_filename( std::string_view filename ) const { return matches_any_wrf_prefix( filename, WRF_RESTART_PREFIXES ); }


std::vector<std::string> WRFControl::get_log_filenames() const { return {}; }


bool WRFControl::setup_directories( const fs::path& slot_path ) const
{
    (void)slot_path;
    return false;
}


ModelControlInputData WRFControl::parse_control_input() const
{
    // Code is based on OpenIFSControl::parse_control_input()
    // but WRF doesn#t have a restart namelist file, so this is a stub for now.
    ModelControlInputData parsed;
    parsed.source_file = control_input_file;

    if ( !fs::exists( control_input_file ) ) {
        return make_parse_error( control_input_file, "exists", "", "model control input file does not exist" );
    }

    std::ifstream control_input_stream( control_input_file );
    if ( !control_input_stream.is_open() ) {
        return make_parse_error( control_input_file, "open", "", "failed to open model control input file" );
    }

    std::string input_line;
    std::string parsed_key;
    std::string parsed_value;
    std::string tmpstr;
    std::string err_msg;
    std::string history_interval;
    std::string frames_per_outfile;
    int restart_interval_minutes = 0;
    long long run_len_secs = 0;

    while ( std::getline( control_input_stream, input_line ) ) {
        trim_whitespace( input_line );
        if ( input_line.empty() ) {
            continue;
        }

        parsed_key.clear();
        parsed_value.clear();

        bool have_kv = false;

        // Ignore comments.
        if ( input_line.front() == '!' ) {
            have_kv = false;
        } else if ( parse_namelist_key_value( input_line, parsed_key, parsed_value ) ) {
            have_kv = true;
        }

        if ( !have_kv ) {
            continue;
        }

        // Parse the 'time_step' parameter (secs) from the namelist.input file.
        // Ignore any decimal point and treat as an integer.
        // Ignore other variables beginning with 'time_step'.

        if ( parsed_key == "time_step" ) {
            tmpstr = parsed_value;
            if ( auto decimal_point = tmpstr.find( '.' ); decimal_point != std::string::npos ) {
                tmpstr = tmpstr.substr( 0, decimal_point );
            }
            if ( !parse_int( tmpstr, parsed.timestep_seconds, err_msg ) ) {
                return make_parse_error( control_input_file, "parse", parsed_key, err_msg );
            }
        }

        // Parse the run length.
        // First, read the run_days, run_hours, run_minutes, and run_seconds parameters from the namelist.input file.
        // Then, convert the total run length to seconds and calculate the total number of model steps based on the timestep interval.
        else if ( parsed_key == "run_days" ) {
            tmpstr = parsed_value;
            int days = 0;
            if ( !parse_int( tmpstr, days, err_msg ) ) {
                return make_parse_error( control_input_file, "parse", parsed_key, err_msg );
            }
            run_len_secs = days * 86400;    // Convert days to seconds
        } else if ( parsed_key == "run_hours" ) {
            tmpstr = parsed_value;
            int hours = 0;
            if ( !parse_int( tmpstr, hours, err_msg ) ) {
                return make_parse_error( control_input_file, "parse", parsed_key, err_msg );
            }
            run_len_secs = run_len_secs + hours * 3600;    // Convert hours to seconds and add to total
        } else if ( parsed_key == "run_minutes" ) {
            tmpstr = parsed_value;
            int minutes = 0;
            if ( !parse_int( tmpstr, minutes, err_msg ) ) {
                return make_parse_error( control_input_file, "parse", parsed_key, err_msg );
            }
            run_len_secs = run_len_secs + minutes * 60;    // Convert minutes to seconds and add to total
        } else if ( parsed_key == "run_seconds" ) {
            tmpstr = parsed_value;
            int seconds = 0;
            if ( !parse_int( tmpstr, seconds, err_msg ) ) {
                return make_parse_error( control_input_file, "parse", parsed_key, err_msg );
            }
            run_len_secs = run_len_secs + seconds;    // Add seconds to total
        }

        // Restart interval 'restart_interval' in mins
        else if ( parsed_key == "restart_interval" ) {
            tmpstr = parsed_value;
            if ( !parse_int( tmpstr, restart_interval_minutes, err_msg ) ) {
                return make_parse_error( control_input_file, "parse", parsed_key, err_msg );
            }
        }

        // Find out how many domains we're using.
        // We'll need this to determine how many output and restart files to expect.
        else if ( parsed_key == "max_dom" ) {
            tmpstr = parsed_value;
            if ( !parse_int( tmpstr, max_domains, err_msg ) ) {
                return make_parse_error( control_input_file, "parse", parsed_key, err_msg );
            }
        }

        //  Output interval.
        //  For now assume we only want to upload the smallest grid. The namelist variables we need are e.g.:
        //     history_interval = 9999, 9999, 60,   ! in minutes
        //     frames_per_outfile = 1, 1, 24,       ! count of output instances; ie. 24x60 = 1 day.
        //  The value of max_dom gives the value to use from these.
        else if ( parsed_key == "history_interval" ) {
            if ( !extract_namelist_rhs_preserving_list( input_line, history_interval ) ) {
                return make_parse_error( control_input_file, "parse", parsed_key, "failed to extract namelist value list" );
            }
        } else if ( parsed_key == "frames_per_outfile" ) {
            if ( !extract_namelist_rhs_preserving_list( input_line, frames_per_outfile ) ) {
                return make_parse_error( control_input_file, "parse", parsed_key, "failed to extract namelist value list" );
            }
        }
    }

    // Compute output interval in model steps for the smallest grid.
    // By 'output_interval', we mean the frequency at which new model output files are created,
    // which is not the rate at which WRF output fields are written to that file.
    if ( parsed.timestep_seconds <= 0 ) {
        return make_parse_error( control_input_file, "validate", "time_step", "time_step must be a positive integer" );
    }

    int domain_history_interval = 0;
    int domain_frames_per_outfile = 0;
    if ( !parse_wrf_domain_list_value( history_interval, max_domains, domain_history_interval, err_msg ) ) {
        return make_parse_error( control_input_file, "parse", "history_interval", err_msg );
    }
    if ( !parse_wrf_domain_list_value( frames_per_outfile, max_domains, domain_frames_per_outfile, err_msg ) ) {
        return make_parse_error( control_input_file, "parse", "frames_per_outfile", err_msg );
    }
    parsed.output_interval = ( domain_history_interval * domain_frames_per_outfile * 60 ) / parsed.timestep_seconds;
    parsed.restart_interval = ( restart_interval_minutes * 60 ) / parsed.timestep_seconds;

    // Compute remaining time related variables
    parsed.total_steps = static_cast<decltype( parsed.total_steps )>( run_len_secs / static_cast<long long>( parsed.timestep_seconds ) );
    parsed.forecast_length_time = static_cast<double>( run_len_secs );

    std::cerr << "WRF namelist.input parsed successfully:\n"
              << "Timestep (secs)=" << parsed.timestep_seconds << ", total_steps=" << parsed.total_steps
              << ", output_interval=" << parsed.output_interval << ", forecast_length_time=" << parsed.forecast_length_time
              << ", max_domains=" << max_domains << ", restart interval=" << parsed.restart_interval << ", output_interval=" << parsed.output_interval
              << '\n';

    parsed.ok = true;
    return parsed;
}


bool WRFControl::restart_ctl_exists() const { return false; }


bool WRFControl::restart_ctl_read( std::string& step, std::string& time ) const
{
    step.clear();
    time.clear();
    return false;
}


bool WRFControl::setup( const fs::path& slot_path ) const
{
    (void)slot_path;
    return true;
}


bool WRFControl::do_step_tasks( int current_step, const fs::path& slot_path )
{
    (void)current_step;
    (void)slot_path;
    return true;
}

//
// Implementation of the WRF 4.6.1 Urban model control class.
//   Glenn Carver, CPDN, 2026.

#include "wrf_control.h"

#include "../../lib/utils.h"
#include <cctype>

namespace {

constexpr std::string_view WRF_OUTPUT_PREFIX = "wrfout_d03_";
constexpr std::string_view WRF_RESTART_PREFIX = "wrfrst_d03_";

std::vector<std::string> wrf_get_omp_env_vars( const std::string& nthreads )
{
    return { "OMP_NUM_THREADS=" + nthreads };
}

bool is_ascii_digit( char ch ) { return std::isdigit( static_cast<unsigned char>( ch ) ) != 0; }

bool is_wrf_datetime_suffix( std::string_view suffix )
{
    if ( suffix.size() != 19 ) {
        return false;
    }

    constexpr int digit_positions[] = { 0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18 };
    for ( int pos : digit_positions ) {
        if ( !is_ascii_digit( suffix[static_cast<std::size_t>( pos )] ) ) {
            return false;
        }
    }

    return suffix[4] == '-' && suffix[7] == '-' && suffix[10] == '_' && suffix[13] == ':' && suffix[16] == ':';
}

bool is_wrf_prefixed_datetime_filename( std::string_view filename, std::string_view prefix )
{
    return filename.size() == prefix.size() + 19 && filename.rfind( prefix, 0 ) == 0 &&
           is_wrf_datetime_suffix( filename.substr( prefix.size() ) );
}

}    // namespace

void WRFControl::print_logs( const int nlines ) const
{
    (void)nlines;
}

bool WRFControl::check_model_success() const
{
    return false;
}

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

std::vector<std::string> WRFControl::get_output_filenames( int step, std::string_view id ) const
{
    (void)step;
    (void)id;
    return {};
}

bool WRFControl::is_output_filename( std::string_view filename ) const
{
    return is_wrf_prefixed_datetime_filename( filename, WRF_OUTPUT_PREFIX );
}

bool WRFControl::is_restart_filename( std::string_view filename ) const
{
    return is_wrf_prefixed_datetime_filename( filename, WRF_RESTART_PREFIX );
}

std::vector<std::string> WRFControl::get_log_filenames() const
{
    return {};
}

bool WRFControl::setup_directories( const fs::path& slot_path ) const
{
    (void)slot_path;
    return false;
}

ModelControlInputData WRFControl::parse_control_input() const
{
    ModelControlInputData parsed;
    parsed.ok = false;
    parsed.error_step = "not_implemented";
    parsed.error_message = "WRFControl::parse_control_input is not implemented";
    return parsed;
}

bool WRFControl::restart_ctl_exists() const
{
    return false;
}

bool WRFControl::restart_ctl_read( std::string& step, std::string& time ) const
{
    step.clear();
    time.clear();
    return false;
}

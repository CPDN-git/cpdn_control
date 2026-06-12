//
// Implementation of the WRF 4.6.1 Urban model control class.
//   Glenn Carver, CPDN, 2026.

#include "wrf_control.h"

#include "../../lib/utils.h"
#include <array>
#include <cctype>

namespace {

// These are fixed-size compile-time lookup tables, so std::array is the right tool:
// it keeps the size in the type and avoids the dynamic allocation/extra machinery that std::vector would add.
constexpr std::array<std::string_view, 3> WRF_OUTPUT_PREFIXES = { "wrfout_d01_", "wrfout_d02_", "wrfout_d03_" };
constexpr std::array<std::string_view, 3> WRF_RESTART_PREFIXES = { "wrfrst_d01_", "wrfrst_d02_", "wrfrst_d03_" };

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
    return filename.size() == prefix.size() + 19 && filename.rfind( prefix, 0 ) == 0 &&
           is_wrf_datetime_suffix( filename.substr( prefix.size() ) );
}

template <std::size_t N>
bool matches_any_wrf_prefix( std::string_view filename, const std::array<std::string_view, N>& prefixes )
{
    for ( std::string_view prefix : prefixes ) {
        if ( is_wrf_prefixed_datetime_filename( filename, prefix ) ) {
            return true;
        }
    }
    return false;
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
    return matches_any_wrf_prefix( filename, WRF_OUTPUT_PREFIXES );
}

bool WRFControl::is_restart_filename( std::string_view filename ) const
{
    return matches_any_wrf_prefix( filename, WRF_RESTART_PREFIXES );
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

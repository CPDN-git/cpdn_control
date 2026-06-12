//
// Implementation of the WRF 4.6.1 Urban model control class.
//   Glenn Carver, CPDN, 2026.

#include "wrf_control.h"

#include "../../lib/utils.h"

namespace {

std::vector<std::string> wrf_get_omp_env_vars( const std::string& nthreads )
{
    return { "OMP_NUM_THREADS=" + nthreads };
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

std::vector<std::string> WRFControl::get_log_filenames() const
{
    return {};
}

std::regex WRFControl::get_output_filename_regex() const
{
    return std::regex( "$^" );
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

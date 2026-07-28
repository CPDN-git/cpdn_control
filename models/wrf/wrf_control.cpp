//
// Implementation of the WRF model control class.
//   Glenn Carver, CPDN, 2026.

#include "wrf_control.h"

#include "../../lib/utils.h"
#include "wrf_datetime.h"
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>


// Helpers
namespace {

std::vector<std::string> wrf_get_omp_env_vars( const std::string& nthreads ) { return { "OMP_NUM_THREADS=" + nthreads }; }


std::vector<std::string> build_wrf_domain_prefixes( std::string_view file_stem, int domain_count )
{
    if ( domain_count <= 0 ) {
        return {};
    }

    std::vector<std::string> prefixes;
    prefixes.reserve( static_cast<std::size_t>( domain_count ) );

    for ( int domain_index = 1; domain_index <= domain_count; ++domain_index ) {
        std::array<char, 16> prefix_buffer{};
        const int prefix_len = std::snprintf( prefix_buffer.data(), prefix_buffer.size(), "%.*s_d%02d_", static_cast<int>( file_stem.size() ),
                                              file_stem.data(), domain_index );
        if ( prefix_len <= 0 || prefix_len >= static_cast<int>( prefix_buffer.size() ) ) {
            return {};
        }

        prefixes.emplace_back( prefix_buffer.data(), static_cast<std::size_t>( prefix_len ) );
    }

    return prefixes;
}


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

    const bool date_separators_ok = suffix[4] == '-' && suffix[7] == '-' && suffix[10] == '_';
#if defined( _WIN32 )
    // Windows cannot create filenames containing ':'.
    // This fallback accepts a Windows-safe HH-MM-SS timestamp, but WRF itself still needs a
    // corresponding code change to emit '-' instead of ':' in restart/output filename datestamps.
    const bool time_separators_ok = ( suffix[13] == ':' || suffix[13] == '-' ) && ( suffix[16] == ':' || suffix[16] == '-' );
#else
    const bool time_separators_ok = suffix[13] == ':' && suffix[16] == ':';
#endif

    return date_separators_ok && time_separators_ok;
}


bool is_wrf_prefixed_datetime_filename( std::string_view filename, std::string_view prefix )
{
    return filename.size() == prefix.size() + 19 && filename.rfind( prefix, 0 ) == 0 && is_wrf_datetime_suffix( filename.substr( prefix.size() ) );
}


bool parse_wrf_timestamp( std::string_view text, DateTime& datetime )
{
    if ( !is_wrf_datetime_suffix( text ) ) {
        return false;
    }

    datetime.year = ( text[0] - '0' ) * 1000 + ( text[1] - '0' ) * 100 + ( text[2] - '0' ) * 10 + ( text[3] - '0' );
    datetime.month = ( text[5] - '0' ) * 10 + ( text[6] - '0' );
    datetime.day = ( text[8] - '0' ) * 10 + ( text[9] - '0' );
    datetime.hour = ( text[11] - '0' ) * 10 + ( text[12] - '0' );
    datetime.minute = ( text[14] - '0' ) * 10 + ( text[15] - '0' );
    datetime.second = ( text[17] - '0' ) * 10 + ( text[18] - '0' );
    return true;
}


bool parse_wrf_timing_line( const std::string& line, int& domain, DateTime& timestamp )
{
    constexpr std::string_view prefix = "Timing for main: time ";
    constexpr std::string_view domain_marker = " on domain";

    if ( line.rfind( prefix.data(), 0 ) != 0 ) {
        return false;
    }

    const std::size_t timestamp_pos = prefix.size();
    if ( line.size() < timestamp_pos + 19 ) {
        return false;
    }

    if ( !parse_wrf_timestamp( std::string_view( line ).substr( timestamp_pos, 19 ), timestamp ) ) {
        return false;
    }

    const std::size_t domain_pos = line.find( domain_marker.data(), timestamp_pos + 19 );
    if ( domain_pos == std::string::npos ) {
        return false;
    }

    const std::size_t domain_text_pos = domain_pos + domain_marker.size();
    const std::size_t colon_pos = line.find( ':', domain_text_pos );
    if ( colon_pos == std::string::npos ) {
        return false;
    }

    std::string domain_text = line.substr( domain_text_pos, colon_pos - domain_text_pos );
    trim_whitespace( domain_text );
    if ( domain_text.empty() ) {
        return false;
    }

    std::string err_msg;
    return parse_int( domain_text, domain, err_msg );
}

/**
 * @brief This function is required because the default parse code will strip any 
 *        remaining characters beyond the first comma. It was coded for OpenIFS and did not
 *        expect multiple values per line. Ideally the parse fn needs recoding.
 *        For now this is a workaround. It preserves the right-hand-side of the namelist name=value pair.
 */
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


int* get_wrf_datetime_field( DateTime& datetime, std::string_view parsed_key )
{
    if ( parsed_key == "start_year" || parsed_key == "end_year" ) {
        return &datetime.year;
    }
    if ( parsed_key == "start_month" || parsed_key == "end_month" ) {
        return &datetime.month;
    }
    if ( parsed_key == "start_day" || parsed_key == "end_day" ) {
        return &datetime.day;
    }
    if ( parsed_key == "start_hour" || parsed_key == "end_hour" ) {
        return &datetime.hour;
    }
    if ( parsed_key == "start_minute" || parsed_key == "end_minute" ) {
        return &datetime.minute;
    }
    if ( parsed_key == "start_second" || parsed_key == "end_second" ) {
        return &datetime.second;
    }

    return nullptr;
}

/**
 * @brief Used to check wrf filename is a restart or output file
 */
bool matches_any_wrf_prefix( std::string_view filename, const std::vector<std::string>& prefixes )
{
    for ( const auto& prefix : prefixes ) {
        if ( is_wrf_prefixed_datetime_filename( filename, prefix ) ) {
            return true;
        }
    }
    return false;
}


bool extract_wrf_restart_timestamp( std::string_view filename, const std::vector<std::string>& prefixes, std::string& timestamp )
{
    timestamp.clear();

    for ( const auto& prefix : prefixes ) {
        if ( !is_wrf_prefixed_datetime_filename( filename, prefix ) ) {
            continue;
        }

        const std::string candidate_timestamp( filename.substr( prefix.size() ) );
        DateTime parsed_timestamp{};
        if ( !parse_wrf_timestamp( candidate_timestamp, parsed_timestamp ) ) {
            return false;
        }

        timestamp = candidate_timestamp;
        return true;
    }

    return false;
}


std::string build_wrf_start_value_list( int value, int width, int domain_count )
{
    std::ostringstream rhs_stream;

    for ( int domain_index = 0; domain_index < domain_count; ++domain_index ) {
        if ( domain_index > 0 ) {
            rhs_stream << ", ";
        }
        rhs_stream << std::setw( width ) << std::setfill( '0' ) << value;
    }
    rhs_stream << ',';

    return rhs_stream.str();
}


/**
 * @brief Replace the right-hand-side of a namelist line while preserving any comment suffix.
 * @param line The original line from the namelist.
 * @param rhs The new right-hand-side value to replace in the line.
 * @return A new string with the right-hand-side replaced, preserving any comment suffix.
 */
std::string replace_namelist_rhs( const std::string& line, const std::string& rhs )
{
    const auto equals_pos = line.find( '=' );
    if ( equals_pos == std::string::npos ) {
        return line;
    }

    const auto comment_pos = line.find( '!', equals_pos + 1 );
    const std::string comment_suffix = comment_pos == std::string::npos ? std::string() : line.substr( comment_pos );

    std::string updated_line = line.substr( 0, equals_pos + 1 );
    updated_line += ' ';
    updated_line += rhs;
    if ( !comment_suffix.empty() ) {
        updated_line += ' ';
        updated_line += comment_suffix;
    }

    return updated_line;
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
    constexpr std::string_view success_marker = "wrf: SUCCESS COMPLETE WRF";

    // Check for 'success' string in WRF output.
    // WRF writes this success marker to stdout. The controller redirects the model
    // child stdout to stdout.txt in the slot directory. Scan the full file because
    // the success line may not be last.

    if ( fs::exists( model_log ) ) {
        std::ifstream log_stream( model_log );
        if ( !log_stream.is_open() ) {
            std::cerr << "Warning! Could not open model log : " << model_log << '\n';
            return false;
        }

        std::string line{};
        while ( std::getline( log_stream, line ) ) {
            if ( line.rfind( success_marker, 0 ) == 0 ) {
                success = true;
                break;
            }
        }

        log_stream.close();

        if ( success ) {
            std::cerr << "Found: " << success_marker << ", in model log. Model succeeded." << '\n';
        } else {
            std::cerr << "Didn't find: " << success_marker << ", in model log. Model failed." << '\n';
        }
    } else {
        std::cerr << "Warning! Could not find model log : " << model_log << '\n';
    }

    return success;
}


/**
 * @brief WRF does not have any additional controller-managed log files to print here.
 */
void WRFControl::print_logs( const int nlines ) const
{
    print_last_lines( model_log, nlines );    // from lib/utils.h; will check file exists
}


/**
 * @brief Get list of logical model input files to unpack from project directory as
 *        delivered from CPDN servers. So these are the *packed* files.
 *        For WRF these will be the model initial conditions and boundary data files
 *        together with the 'run' files; climatologies etc.
 */
ModelInputManifest WRFControl::get_input_manifest( const std::string& workunit_id ) const
{
    // The controller keeps BOINC filename resolution, checksum verification and staging generic.
    // This function declares the logical BOINC files it expects from the CPDN server and where
    // each archive unpacks.
    // NOTE!! These filenames are preliminary and assume the WRF input files are packed as:
    //    ic_ancil  : wrfinput_d*, wrfbdy_d01
    //    rundata : what WRF doc calls the 'run' files; essentially climatologies:
    //                CAMtr_volume_mixing_ratio*, *.TBL, ozone*, RRTMG_LW/SW_DATA
    //    Both these unpack into the slot directory, no subdirs used.
    //
    //    The namelist.input and accompanying iofields_d*.txt files are packed in the wu zip.
    return {
        { "ic_ancil_" + workunit_id + ".zip", fs::path( "." ) },
        { "rundata_" + workunit_id + ".zip", fs::path( "." ) },
    };
}


/**
 * @brief Provide runtime environment variables.
 *        The only one WRF uses it OMP_NUM_THREADS
 */
std::vector<std::string> WRFControl::get_env_vars( const std::string& slot_path, const std::string& nthreads, std::string& err_msg ) const
{
    err_msg.clear();

    if ( std::string nthreads_copy = nthreads; !parse_int( nthreads_copy ) ) {
        err_msg = "invalid value of 'nthreads': " + nthreads;
        return {};
    }

    return wrf_get_omp_env_vars( nthreads );
}


bool WRFControl::read_validated_max_domains( int& parsed_max_domains, std::string& err_msg ) const
{
    err_msg.clear();
    parsed_max_domains = 0;

    if ( !fs::exists( control_input_file ) ) {
        err_msg = "model control input file does not exist";
        return false;
    }

    std::ifstream control_input_stream( control_input_file );
    if ( !control_input_stream.is_open() ) {
        err_msg = "failed to open model control input file";
        return false;
    }

    std::string input_line;
    std::string parsed_key;
    std::string parsed_value;

    while ( std::getline( control_input_stream, input_line ) ) {
        trim_whitespace( input_line );
        if ( input_line.empty() || input_line.front() == '!' ) {
            continue;
        }

        parsed_key.clear();
        parsed_value.clear();
        if ( !parse_namelist_key_value( input_line, parsed_key, parsed_value ) || parsed_key != "max_dom" ) {
            continue;
        }

        std::string value_text = parsed_value;
        if ( !parse_int( value_text, parsed_max_domains, err_msg ) ) {
            return false;
        }

        if ( parsed_max_domains <= 0 || parsed_max_domains > max_dom_allowed ) {
            err_msg = "max_dom must be between 1 and " + std::to_string( max_dom_allowed );
            return false;
        }

        return true;
    }

    err_msg = "missing required field max_dom";
    return false;
}


void WRFControl::set_domain_prefixes( int domain_count ) const
{
    output_prefixes = build_wrf_domain_prefixes( "wrfout", domain_count );
    restart_prefixes = build_wrf_domain_prefixes( "wrfrst", domain_count );
}


void WRFControl::clear_restart_scan_cache() const
{
    restart_sets.clear();
    restart_scan_cached = false;
    cached_restart_exists = false;
    cached_restart_scan_dir.clear();
    cached_restart_scan_max_domains = 0;
    restart_reference_start_datetime = {};
    restart_reference_start_valid = false;
}


bool WRFControl::ensure_domain_prefixes_initialized() const
{
    if ( max_domains > 0 && max_domains <= max_dom_allowed && output_prefixes.size() == static_cast<std::size_t>( max_domains ) &&
         restart_prefixes.size() == static_cast<std::size_t>( max_domains ) ) {
        return true;
    }

    std::string err_msg;
    int parsed_max_domains = 0;
    if ( !read_validated_max_domains( parsed_max_domains, err_msg ) ) {
        return false;
    }

    max_domains = parsed_max_domains;
    set_domain_prefixes( max_domains );

    return output_prefixes.size() == static_cast<std::size_t>( max_domains ) && restart_prefixes.size() == static_cast<std::size_t>( max_domains );
}


const WRFControl::RestartSet* WRFControl::find_latest_valid_restart_set() const
{
    const RestartSet* latest_restart_set = nullptr;

    for ( const auto& restart_set : restart_sets ) {
        if ( restart_set.domains_present.size() != static_cast<std::size_t>( max_domains ) ) {
            continue;
        }

        bool all_domains_present = true;
        for ( bool domain_present : restart_set.domains_present ) {
            if ( !domain_present ) {
                all_domains_present = false;
                break;
            }
        }

        if ( !all_domains_present ) {
            continue;
        }

        if ( latest_restart_set == nullptr || restart_set.datetime > latest_restart_set->datetime ) {
            latest_restart_set = &restart_set;
        }
    }

    return latest_restart_set;
}


/**
 * @brief Update the namelist.input file to set the restart flag and update the start date/time values.
 * @param restart_set The restart set containing the datetime and domain presence information.
 * @returns True if the namelist was successfully updated, otherwise false.
 */
bool WRFControl::update_restart_namelist( const RestartSet& restart_set ) const
{
    DateTime restart_file_datetime{};
    if ( !parse_wrf_timestamp( restart_set.datetime, restart_file_datetime ) ) {
        std::cerr << "update_restart_namelist : Failed to parse WRF restart timestamp to update namelist.input: " << restart_set.datetime << '\n';
        return false;
    }
    DateTime start_datetime{};    // This is the current datetime in the namelist.input; it may NOT be the original start time if multiple restarts.

    int original_start_fields_parsed = 0;

    std::ifstream input_stream( control_input_file );
    if ( !input_stream.is_open() ) {
        std::cerr << "update_restart_namelist: Failed to open WRF control file for restart update: " << control_input_file << '\n';
        return false;
    }

    std::vector<std::string> lines;
    std::string line;
    bool restart_key_found = false;
    bool file_changed = false;
    std::size_t time_control_line_index = std::string::npos;
    int start_keys_updated = 0;

    // Scan the namelist.input file, find restart flag and start date/time keys and update them.
    std::cerr << "update_restart_namelist: Scanning namelist.input to adjust restart variables... " << restart_set.datetime << '\n';
    while ( std::getline( input_stream, line ) ) {
        std::string updated_line = line;
        std::string trimmed_line = line;
        trim_whitespace( trimmed_line );

        if ( trimmed_line == "&time_control" && time_control_line_index == std::string::npos ) {
            time_control_line_index = lines.size();
        }

        // Match the namelist restart key on the left-hand side while ignoring spacing and comments.
        std::string uncommented = line;
        if ( const auto comment_pos = uncommented.find( '!' ); comment_pos != std::string::npos ) {
            uncommented = uncommented.substr( 0, comment_pos );
        }

        if ( const auto equals_pos = uncommented.find( '=' ); equals_pos != std::string::npos ) {
            std::string lhs = uncommented.substr( 0, equals_pos );
            trim_whitespace( lhs );

            if ( lhs == "restart" ) {
                restart_key_found = true;

                const auto false_pos = updated_line.find( ".false." );
                if ( false_pos != std::string::npos ) {
                    updated_line.replace( false_pos, 7, ".true." );
                    file_changed = true;
                    std::cerr << "Updating WRF restart flag to .true. in " << control_input_file << '\n';
                } else if ( updated_line.find( ".true." ) == std::string::npos ) {
                    std::cerr << "WRF restart key found in " << control_input_file
                              << " but value is neither .false. nor .true.; refusing to modify it.\n";
                    return false;
                }
            } else {
                int field_value = 0;
                int field_width = 0;
                int* original_field = get_wrf_datetime_field( start_datetime, lhs );

                if ( lhs == "start_year" ) {
                    field_value = restart_file_datetime.year;
                    field_width = 4;
                } else if ( lhs == "start_month" ) {
                    field_value = restart_file_datetime.month;
                    field_width = 2;
                } else if ( lhs == "start_day" ) {
                    field_value = restart_file_datetime.day;
                    field_width = 2;
                } else if ( lhs == "start_hour" ) {
                    field_value = restart_file_datetime.hour;
                    field_width = 2;
                } else if ( lhs == "start_minute" ) {
                    field_value = restart_file_datetime.minute;
                    field_width = 2;
                } else if ( lhs == "start_second" ) {
                    field_value = restart_file_datetime.second;
                    field_width = 2;
                }

                if ( field_width > 0 ) {
                    if ( original_field != nullptr ) {
                        std::string rhs_text;
                        std::string err_msg;
                        int parsed_original_value = 0;
                        if ( extract_namelist_rhs_preserving_list( line, rhs_text ) &&
                             parse_wrf_domain_list_value( rhs_text, 1, parsed_original_value, err_msg ) ) {
                            *original_field = parsed_original_value;
                            ++original_start_fields_parsed;
                        } else {
                            std::cerr << "Failed to parse original WRF " << lhs
                                      << " value before restart rewrite: " << ( err_msg.empty() ? "unknown error" : err_msg ) << '\n';
                        }
                    }

                    const std::string rhs = build_wrf_start_value_list( field_value, field_width, max_domains );
                    const std::string rewritten_line = replace_namelist_rhs( line, rhs );
                    if ( rewritten_line != line ) {
                        updated_line = rewritten_line;
                        file_changed = true;
                    }
                    ++start_keys_updated;
                }
            }
        }

        lines.push_back( updated_line );
    }
    input_stream.close();

    if ( !restart_key_found ) {
        if ( time_control_line_index == std::string::npos ) {
            std::cerr << "Failed to insert WRF restart setting because &time_control was not found in " << control_input_file << '\n';
            return false;
        }

        lines.insert( lines.begin() + static_cast<std::ptrdiff_t>( time_control_line_index + 1 ), " restart = .true.," );
        file_changed = true;
        std::cerr << "Inserted missing WRF restart setting after &time_control in " << control_input_file << '\n';
    }

    if ( original_start_fields_parsed == 6 && datetime_duration_seconds( start_datetime, start_datetime ) == 0 ) {
        restart_reference_start_datetime = start_datetime;
        restart_reference_start_valid = true;
        std::cerr << "Cached namelist.input start datetimes before restart rewrite.\n";
    } else {
        restart_reference_start_valid = false;
        std::cerr << "Failed to cache original WRF start datetime before restart rewrite; parsed " << original_start_fields_parsed
                  << " start_* fields.\n";
        return false;
    }

    std::cerr << "Prepared WRF restart namelist update for timestamp " << restart_set.datetime << "; updated " << start_keys_updated
              << " start_* keys across " << max_domains << " domains.\n";

    if ( !file_changed ) {
        std::cerr << "WRF restart namelist already matches cached restart timestamp " << restart_set.datetime << " in " << control_input_file << '\n';
        return true;
    }

    fs::path backup_path = control_input_file;
    backup_path += ".bak";
    std::error_code copy_ec;
    fs::copy_file( control_input_file, backup_path, fs::copy_options::overwrite_existing, copy_ec );
    if ( copy_ec ) {
        std::cerr << "Failed to create WRF namelist backup " << backup_path << " before restart rewrite: " << copy_ec.message() << '\n';
        return false;
    }
    std::cerr << "Created WRF namelist backup: " << backup_path << '\n';

    std::ofstream output_stream( control_input_file, std::ios::trunc );
    if ( !output_stream.is_open() ) {
        std::cerr << "Failed to reopen WRF control file for restart update: " << control_input_file << '\n';
        return false;
    }

    for ( std::size_t i = 0; i < lines.size(); ++i ) {
        output_stream << lines[i];
        if ( i + 1 < lines.size() ) {
            output_stream << '\n';
        }
    }

    if ( !output_stream.good() ) {
        std::cerr << "Failed while writing updated WRF control file: " << control_input_file << '\n';
        output_stream.close();
        std::error_code restore_ec;
        fs::copy_file( backup_path, control_input_file, fs::copy_options::overwrite_existing, restore_ec );
        if ( restore_ec ) {
            std::cerr << "Failed to restore WRF namelist from backup " << backup_path << ": " << restore_ec.message() << '\n';
        } else {
            std::cerr << "Restored WRF namelist from backup after write failure: " << backup_path << '\n';
        }
        return false;
    }

    std::cerr << "Updated WRF restart namelist for cached restart timestamp " << restart_set.datetime << " in " << control_input_file << '\n';
    return true;
}


/**
 * @brief Read the WRF log and return the latest completed model step.
 *
 * WRF writes progress messages into `stdout.txt`. We only care about lines that
 * begin with `Timing for main`, and from those we only use the records for
 * domain 1, which is the outermost grid and the one the controller tracks for
 * overall task progress.
 *
 * The timestamp in the latest matching domain-1 line is converted back into
 * elapsed seconds from the model start time. We then divide by the configured
 * timestep length to recover the completed step count expected by `cpdn_main()`.
 * 
 * Issue: On a restart the stdout.txt file could contain later timesteps from the
 * previous run than the restart file. This will cause this function to return
 * a step count that's higher than the restart timestamp. It will only happen
 * on the first call after a restart. Potentially it's a problem if it causes
 * the controller to trigger a trickle. The fix needs to be in this routine ideally.
 * The right fix would be to ignore any log lines that are later than the restart timestamp. 
 * But that requires the restart timestamp to be known here. TO DO.
 */
bool WRFControl::get_current_step( int& step, const int total_steps ) const
{
    step = 0;

    // The namelist parser must already have filled in the timestep.
    if ( timestep_seconds <= 0 ) {
        return false;
    }

    // WRF writes its progress timing lines to stdout.txt in the slot directory.
    std::ifstream log_stream( "stdout.txt" );
    if ( !log_stream.is_open() ) {
        return false;
    }

    std::string line;
    DateTime latest_domain1_timestamp{};
    bool found_domain1_timing = false;

    // Read the log from top to bottom and keep replacing the saved timestamp.
    // That means the final saved value is the latest domain-1 timing record.
    while ( std::getline( log_stream, line ) ) {
        int domain = 0;
        DateTime timestamp{};
        if ( !parse_wrf_timing_line( line, domain, timestamp ) ) {
            continue;
        }
        // Ignore timing lines for the nested domains. Progress is tracked on domain 1 only.
        if ( domain != 1 ) {
            continue;
        }

        latest_domain1_timestamp = timestamp;
        found_domain1_timing = true;
    }

    if ( !found_domain1_timing ) {
        return false;
    }

    // Convert the log timestamp back into "seconds since model start".
    const long long elapsed_seconds = datetime_duration_seconds( step0_datetime, latest_domain1_timestamp );
    if ( elapsed_seconds < 0 || ( elapsed_seconds % static_cast<long long>( timestep_seconds ) ) != 0 ) {
        return false;
    }

    // Step count is just elapsed model time divided by seconds per step.
    const long long computed_step = elapsed_seconds / static_cast<long long>( timestep_seconds );
    // Reject impossible values so callers do not act on corrupt or partial log data.
    if ( computed_step < 0 || computed_step > total_steps ) {
        return false;
    }

    step = static_cast<int>( computed_step );
    return true;
}


/**
 * @brief WRF uses date-time stamped output files, return list given the step count
 * 
 * This is not the ideal way for WRF to work. Rather than convert from date-time to steps
 * it would be more natural for the model instance to use an internal date-time diff rather
 * than keep converting. TO DO.
 * Important: The start date-time in the namelist.input will be changed on each restart,
 * so to compute time from the very start use the 'step0' date-time which is read from the original namelist.input.
 */
std::vector<std::string> WRFControl::get_output_filenames( int step ) const
{
    // Take the model step count, compute a time difference and add to the start time.
    if ( step < 0 || timestep_seconds <= 0 || !ensure_domain_prefixes_initialized() || max_domains <= 0 ) {
        return {};
    }

    // Calculate duration and add to the start time (step 0 at initial start).
    const long long elapsed_seconds = static_cast<long long>( step ) * static_cast<long long>( timestep_seconds );

    DateTime duration = secs_to_datetime_duration( elapsed_seconds );
    DateTime output_date = add_duration_to_datetime( step0_datetime, duration );

    // WRF output files use a fixed YYYY-MM-DD_HH:MM:SS suffix.
    std::array<char, 20> timestamp_buffer{};
    const int timestamp_len = std::snprintf( timestamp_buffer.data(), timestamp_buffer.size(), "%04d-%02d-%02d_%02d:%02d:%02d", output_date.year,
                                             output_date.month, output_date.day, output_date.hour, output_date.minute, output_date.second );
    if ( timestamp_len != 19 ) {
        return {};
    }

    std::string timestamp( timestamp_buffer.data(), static_cast<std::size_t>( timestamp_len ) );

    // Emit either all or one output filename per WRF domain for the computed timestamp.
    auto all_domains = false;    // CHANGE to 'true' to emit all domains

    std::vector<std::string> output_filenames;

    if ( all_domains ) {
        const int domain_count = max_domains;
        output_filenames.reserve( static_cast<std::size_t>( domain_count ) );
        for ( int i = 0; i < domain_count; ++i ) {
            output_filenames.emplace_back( output_prefixes[static_cast<std::size_t>( i )] + timestamp );
        }
    } else {
        // Output innermost domain only: WRF domains are numbered from 1, but the
        // cached prefixes vector is zero-based.
        const std::size_t domain_index = static_cast<std::size_t>( max_domains - 1 );
        if ( domain_index >= output_prefixes.size() ) {
            return {};
        }
        output_filenames.emplace_back( output_prefixes[domain_index] + timestamp );
    }

    return output_filenames;
}

std::vector<std::string> WRFControl::get_copyable_output_filenames( int current_step ) const
{
    std::vector<std::string> output_files;
    if ( current_step < 0 || output_interval <= 0 ) {
        return output_files;
    }

    // Return the model output files considered safe to copy as of the current timestep.
    // Each output file becomes complete only after one full output interval has elapsed
    // beyond the step that starts it. For example, with an output_interval of 24 steps,
    // the initial output file (step 0) is copyable once current_step reaches 24, and the
    // next output file (step 24) becomes copyable once current_step reaches 48.
    const int completed_output_count = current_step / output_interval;
    static const std::size_t file_count = get_output_filenames( 0 ).size();    // make static to only get size once
    output_files.reserve( static_cast<std::size_t>( completed_output_count ) * file_count );

    for ( int output_step = 0; output_step < current_step; output_step += output_interval ) {
        const int completed_step = output_step + output_interval;
        if ( completed_step > current_step ) {
            continue;
        }

        auto step_files = get_output_filenames( output_step );
        if ( step_files.empty() ) {
            continue;
        }
        output_files.insert( output_files.end(), step_files.begin(), step_files.end() );
    }

    return output_files;
}


bool WRFControl::is_output_filename( std::string_view filename ) const
{
    if ( !ensure_domain_prefixes_initialized() ) {
        return false;
    }
    return matches_any_wrf_prefix( filename, output_prefixes );
}


bool WRFControl::is_restart_filename( std::string_view filename ) const
{
    if ( !ensure_domain_prefixes_initialized() ) {
        return false;
    }
    return matches_any_wrf_prefix( filename, restart_prefixes );
}


// Upload WRF stdout.txt as the model runtime log; BOINC/controller stderr remains separate.
std::vector<std::string> WRFControl::get_log_filenames() const { return { "stdout.txt" }; }


// WRF doesn't need any separate subdirectories.
bool WRFControl::setup_directories( const fs::path& slot_path ) const
{
    (void)slot_path;
    return false;
}


/**
 * @brief  Read the WRF control file (namelist.input) and extract all the
 *         variables we need to manage the forecast.
 */
ModelControlInputData WRFControl::parse_control_input() const
{
    // Code is based on OpenIFSControl::parse_control_input().
    // Might be scope later for extracting common code patterns.
    ModelControlInputData parsed;

    // On a new model start, the only control file will be the 'namelist.input' file.
    // The 'setup()' function called before this will copy this into the 'step0' file
    // to preserve it so the restart code can see the original start date-time. This allows
    // the controller to measure the model run from the start which keeps trickles correct.
    // So, by the time we get here, we should have the control_input_file_step0 available.
    // However, if we don't fall back to the namelist.input but this is likely an error.
    if ( fs::exists( control_input_file_step0 ) ) {
        parsed.source_file = control_input_file_step0;
    } else {
        parsed.source_file = control_input_file;
        std::cerr << "Warning! WRF control input file step0 not found; using namelist.input instead: " << control_input_file << '\n';
    }
    if ( !fs::exists( parsed.source_file ) ) {
        return make_parse_error( parsed.source_file, "exists", "", "model control input file does not exist" );
    }

    std::ifstream control_input_stream( parsed.source_file );
    if ( !control_input_stream.is_open() ) {
        return make_parse_error( parsed.source_file, "open", "", "failed to open model control input file" );
    }

    std::string input_line;
    std::string parsed_key;
    std::string parsed_value;
    std::string tmpstr;
    std::string err_msg;
    std::string history_interval;
    std::string frames_per_outfile;
    int restart_interval_minutes = 0;
    int parsed_max_domains = 0;
    bool have_start_datetime = false;
    bool have_end_datetime = false;
    DateTime start_datetime{};
    DateTime end_datetime{};

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
                return make_parse_error( parsed.source_file, "parse", parsed_key, err_msg );
            }
            timestep_seconds = parsed.timestep_seconds;    // internal var for later use
        }

        // Get the run start/end date/time used to build output and restart timestamps.
        // WRF lists one value per domain, but the first value is enough because all
        // domains share the same start and end time.
        // DO NOT use the run_* variables in the namelist as these are usually set to
        // zero as they are only used for the initial conditions and not for the actual run.
        // If they are set, they override the end_* variables which causes problems for restarts.
        else if ( parsed_key.rfind( "start_", 0 ) == 0 || parsed_key.rfind( "end_", 0 ) == 0 ) {
            DateTime* datetime_target = nullptr;
            if ( parsed_key.rfind( "start_", 0 ) == 0 ) {
                datetime_target = &start_datetime;
                have_start_datetime = true;
            } else {
                datetime_target = &end_datetime;
                have_end_datetime = true;
            }

            int* datetime_field = get_wrf_datetime_field( *datetime_target, parsed_key );
            if ( datetime_field != nullptr ) {
                tmpstr = parsed_value;
                if ( !parse_int( tmpstr, *datetime_field, err_msg ) ) {
                    return make_parse_error( parsed.source_file, "parse", parsed_key, err_msg );
                }
            }
        }

        // Restart interval 'restart_interval' in mins
        else if ( parsed_key == "restart_interval" ) {
            tmpstr = parsed_value;
            if ( !parse_int( tmpstr, restart_interval_minutes, err_msg ) ) {
                return make_parse_error( parsed.source_file, "parse", parsed_key, err_msg );
            }
        }

        // Find out how many domains we're using.
        // We'll need this to determine how many output and restart files to expect.
        else if ( parsed_key == "max_dom" ) {
            tmpstr = parsed_value;
            if ( !parse_int( tmpstr, parsed_max_domains, err_msg ) ) {
                return make_parse_error( parsed.source_file, "parse", parsed_key, err_msg );
            }
            if ( parsed_max_domains <= 0 || parsed_max_domains > max_dom_allowed ) {
                return make_parse_error( parsed.source_file, "validate", parsed_key,
                                         "max_dom must be between 1 and " + std::to_string( max_dom_allowed ) );
            }
        }

        //  Output interval.
        //  For now assume we only want to upload the smallest grid. The namelist variables we need are e.g.:
        //     history_interval = 9999, 9999, 60,   ! in minutes
        //     frames_per_outfile = 1, 1, 24,       ! count of output instances; ie. 24x60 = 1 day.
        //  The value of max_dom gives the value to use from these.
        else if ( parsed_key == "history_interval" ) {
            if ( !extract_namelist_rhs_preserving_list( input_line, history_interval ) ) {
                return make_parse_error( parsed.source_file, "parse", parsed_key, "failed to extract namelist value list" );
            }
        } else if ( parsed_key == "frames_per_outfile" ) {
            if ( !extract_namelist_rhs_preserving_list( input_line, frames_per_outfile ) ) {
                return make_parse_error( parsed.source_file, "parse", parsed_key, "failed to extract namelist value list" );
            }
        }
    }

    // Compute output interval in model steps for the smallest grid.
    // By 'output_interval', we mean the frequency at which new model output files are created,
    // which is not the rate at which WRF output fields are written to that file.
    if ( parsed.timestep_seconds <= 0 ) {
        return make_parse_error( parsed.source_file, "validate", "time_step", "time_step must be a positive integer" );
    }
    if ( parsed_max_domains <= 0 ) {
        return make_parse_error( parsed.source_file, "validate", "max_dom", "max_dom must be between 1 and " + std::to_string( max_dom_allowed ) );
    }

    max_domains = parsed_max_domains;
    set_domain_prefixes( max_domains );

    int domain_history_interval = 0;
    int domain_frames_per_outfile = 0;
    if ( !parse_wrf_domain_list_value( history_interval, max_domains, domain_history_interval, err_msg ) ) {
        return make_parse_error( parsed.source_file, "parse", "history_interval", err_msg );
    }
    if ( !parse_wrf_domain_list_value( frames_per_outfile, max_domains, domain_frames_per_outfile, err_msg ) ) {
        return make_parse_error( parsed.source_file, "parse", "frames_per_outfile", err_msg );
    }
    parsed.output_interval = ( domain_history_interval * domain_frames_per_outfile * 60 ) / parsed.timestep_seconds;
    output_interval = parsed.output_interval;
    parsed.restart_interval = ( restart_interval_minutes * 60 ) / parsed.timestep_seconds;

    // Compute remaining time related variables
    if ( !have_start_datetime || !have_end_datetime ) {
        return make_parse_error( parsed.source_file, "validate", "time_control", "required start_* and end_* datetime fields were not found" );
    }

    step0_datetime = start_datetime;

    const long long run_len_secs = datetime_duration_seconds( step0_datetime, end_datetime );
    if ( run_len_secs < 0 ) {
        return make_parse_error( parsed.source_file, "validate", "time_control", "forecast end time precedes start time" );
    }
    if ( ( run_len_secs % static_cast<long long>( parsed.timestep_seconds ) ) != 0 ) {
        return make_parse_error( parsed.source_file, "validate", "time_control", "forecast length is not an exact multiple of the timestep" );
    }

    parsed.total_steps = static_cast<decltype( parsed.total_steps )>( run_len_secs / static_cast<long long>( parsed.timestep_seconds ) );
    parsed.forecast_length_time = static_cast<double>( run_len_secs );

    std::cerr << "WRF namelist.input parsed successfully from input file: " << parsed.source_file << "\n"
              << "Timestep (secs)=" << parsed.timestep_seconds << ", total_steps=" << parsed.total_steps
              << ", output_interval=" << parsed.output_interval << ", forecast_length_time=" << parsed.forecast_length_time
              << ", max_domains=" << max_domains << ", restart interval=" << parsed.restart_interval << '\n';

    parsed.ok = true;
    return parsed;
}


/**
 * @brief Scans current (slot) directory and returns true if a valid set
 *        of WRF restart files is found. A valid set is defined as restart files
 *        for all domains of the same date and non-zero size.
 */
bool WRFControl::restart_exists() const
{
    if ( !ensure_domain_prefixes_initialized() || max_domains <= 0 ) {
        std::cerr << "WRF restart scan skipped because domain prefixes are not initialized.\n";
        return false;
    }

    const fs::path scan_dir = fs::current_path();
    if ( restart_scan_cached && cached_restart_scan_dir == scan_dir && cached_restart_scan_max_domains == max_domains ) {
        return cached_restart_exists;
    }

    clear_restart_scan_cache();

    // Scan the slot directory and group non-empty restart files by their timestamp suffix.
    for ( const auto& entry : fs::directory_iterator( scan_dir ) ) {
        if ( !entry.is_regular_file() ) {
            continue;
        }

        std::error_code ec;
        const auto file_size = entry.file_size( ec );
        if ( ec || file_size == 0 ) {
            continue;
        }

        const std::string filename = entry.path().filename().string();

        for ( int domain_index = 0; domain_index < max_domains; ++domain_index ) {
            const std::string& prefix = restart_prefixes[static_cast<std::size_t>( domain_index )];
            if ( !is_wrf_prefixed_datetime_filename( filename, prefix ) ) {
                continue;
            }

            const std::string datetime = filename.substr( prefix.size() );

            auto restart_set = restart_sets.begin();
            for ( ; restart_set != restart_sets.end(); ++restart_set ) {
                if ( restart_set->datetime == datetime ) {
                    break;
                }
            }

            if ( restart_set == restart_sets.end() ) {
                restart_sets.push_back( RestartSet{ datetime, std::vector<bool>( static_cast<std::size_t>( max_domains ), false ) } );
                restart_set = restart_sets.end() - 1;
            }

            restart_set->domains_present[static_cast<std::size_t>( domain_index )] = true;
            break;
        }
    }

    const RestartSet* latest_restart_set = find_latest_valid_restart_set();
    cached_restart_exists = latest_restart_set != nullptr;
    restart_scan_cached = true;
    cached_restart_scan_dir = scan_dir;
    cached_restart_scan_max_domains = max_domains;

    if ( cached_restart_exists ) {
        std::cerr << "WRF restart scan found a valid restart set at " << latest_restart_set->datetime << " across " << max_domains << " domains.\n";
    } else if ( !restart_sets.empty() ) {
        std::cerr << "WRF restart scan found " << restart_sets.size() << " restart timestamp group(s), but none contain all " << max_domains
                  << " domain files.\n";
    }

    return cached_restart_exists;
}


/**
 * @brief Return the restart step from the cached WRF restart scan.
 *
 * Restart scanning takes place earlier via restart_exists(), typically from setup().
 */
bool WRFControl::parse_restart( std::string& step ) const
{
    step.clear();

    if ( restart_sets.empty() ) {
        std::cerr << "WRF parse_restart() called without cached restart scan data. Restart scanning takes place in setup().\n";
        return false;
    }
    if ( !restart_reference_start_valid ) {
        std::cerr << "WRF parse_restart() cannot recover restart step because the pre-rewrite WRF start datetime was not cached in setup().\n";
        return false;
    }
    if ( timestep_seconds <= 0 ) {
        std::cerr << "WRF parse_restart() cannot convert restart timestamp because timestep_seconds is invalid: " << timestep_seconds << '\n';
        return false;
    }

    const RestartSet* latest_restart_set = find_latest_valid_restart_set();
    if ( latest_restart_set == nullptr ) {
        std::cerr << "WRF parse_restart() found cached restart scan data, but no complete restart set across all " << max_domains << " domains.\n";
        return false;
    }

    DateTime restart_datetime{};
    if ( !parse_wrf_timestamp( latest_restart_set->datetime, restart_datetime ) ) {
        std::cerr << "WRF parse_restart() failed to parse cached restart timestamp: " << latest_restart_set->datetime << '\n';
        return false;
    }

    const long long elapsed_seconds = datetime_duration_seconds( restart_reference_start_datetime, restart_datetime );
    if ( elapsed_seconds < 0 ) {
        std::cerr << "WRF parse_restart() computed a negative elapsed time from cached pre-rewrite start datetime to restart timestamp "
                  << latest_restart_set->datetime << '\n';
        return false;
    }
    if ( ( elapsed_seconds % static_cast<long long>( timestep_seconds ) ) != 0 ) {
        std::cerr << "WRF parse_restart() restart timestamp " << latest_restart_set->datetime
                  << " does not fall on an exact timestep boundary for timestep_seconds=" << timestep_seconds << '\n';
        return false;
    }

    const long long restart_step = elapsed_seconds / static_cast<long long>( timestep_seconds );
    if ( restart_step < 0 ) {
        std::cerr << "WRF parse_restart() computed an invalid negative restart step: " << restart_step << '\n';
        return false;
    }

    step = std::to_string( restart_step );
    std::cerr << "WRF parse_restart() mapped restart timestamp " << latest_restart_set->datetime << " to step " << step << '\n';
    return true;
}


/**
 * @brief Run setup tasks before the model is launched.
 *        For WRF this means checking if a valid restart file exists and
 *        if it does change the 'restart' logical in namelist.input, otherwise
 *        WRF will start from initial files again.
 */
bool WRFControl::setup( const fs::path& slot_path ) const
{
    (void)slot_path;

    // Preserve the original starting namelist as controller needs to know the
    // original starting time to compute the restart step later on.
    if ( !fs::exists( control_input_file_step0 ) ) {
        std::error_code copy_ec;
        fs::copy_file( control_input_file, control_input_file_step0, fs::copy_options::overwrite_existing, copy_ec );
        if ( copy_ec ) {
            std::cerr << "WRF setup() failed to preserve initial control input file " << control_input_file << " as " << control_input_file_step0
                      << ": " << copy_ec.message() << '\n';
            return false;
        } else {
            std::cerr << "WRF setup() preserved initial control input file " << control_input_file << " as " << control_input_file_step0 << '\n';
        }
    }

    // If no valid restart set exists, leave namelist.input unchanged.
    if ( !restart_exists() ) {
        return true;
    }

    const RestartSet* latest_restart_set = find_latest_valid_restart_set();
    if ( latest_restart_set == nullptr ) {
        std::cerr << "WRF setup() expected a valid restart set after restart_exists(), but none was cached.\n";
        return false;
    }

    return update_restart_namelist( *latest_restart_set );
}


/**
 * @brief Controls the execution of tasks at each model step.
 * 
 * Task 1: Periodically prune older WRF restart-file sets in the slot directory.
 * WRF writes one restart file per domain for each restart timestamp. To limit disk
 * growth during longer runs, keep only the two most recent non-empty timestamp sets
 * and delete older ones. This is opportunistic housekeeping only: warnings are logged
 * for scan/delete failures, but the controller does not abort the task.
 */
bool WRFControl::do_step_tasks( int current_step, const fs::path& slot_path )
{

    // ----------- Prune restarts -----------

    // Only prune on the requested step delay.
    // May need adjusting.

    constexpr int restart_check_delay = 24;

    if ( current_step <= 0 || current_step % restart_check_delay != 0 ) {
        return true;
    }

    if ( !ensure_domain_prefixes_initialized() ) {
        std::cerr << "Warning! Could not initialize WRF restart prefixes for pruning.\n";
        return false;
    }

    const fs::path scan_dir = slot_path.empty() ? fs::current_path() : slot_path;

    struct RestartFileInfo {
        fs::path path;
        std::string timestamp;
    };

    std::vector<RestartFileInfo> restart_files;
    std::set<std::string> timestamps;

    std::error_code iter_ec;
    fs::directory_iterator dir_iter( scan_dir, iter_ec );
    if ( iter_ec ) {
        std::cerr << "Warning! Could not scan WRF slot directory for restart pruning: " << scan_dir << '\n';
        return false;
    }

    for ( const auto& entry : dir_iter ) {
        if ( !entry.is_regular_file() ) {
            continue;
        }

        std::error_code size_ec;
        const auto file_size = entry.file_size( size_ec );
        if ( size_ec || file_size == 0 ) {
            continue;
        }

        std::string timestamp;
        const std::string filename = entry.path().filename().string();
        if ( !extract_wrf_restart_timestamp( filename, restart_prefixes, timestamp ) ) {
            continue;
        }

        // Collect both the physical file path and its logical restart timestamp.
        timestamps.insert( timestamp );
        restart_files.push_back( RestartFileInfo{ entry.path(), std::move( timestamp ) } );
    }

    // Nothing to prune if there are at most two restart times on disk already.
    if ( timestamps.size() <= 2 ) {
        return true;
    }

    std::set<std::string> timestamps_to_keep;
    auto keep_it = timestamps.rbegin();
    // WRF timestamp strings sort lexically in chronological order.
    for ( int kept_count = 0; keep_it != timestamps.rend() && kept_count < 2; ++keep_it, ++kept_count ) {
        timestamps_to_keep.insert( *keep_it );
    }

    for ( const auto& restart_file : restart_files ) {
        if ( timestamps_to_keep.find( restart_file.timestamp ) != timestamps_to_keep.end() ) {
            continue;
        }

        std::error_code remove_ec;
        fs::remove( restart_file.path, remove_ec );

        // Re-check existence so we can warn if deletion silently failed.
        std::error_code exists_ec;
        const bool still_exists = fs::exists( restart_file.path, exists_ec );
        if ( !remove_ec && !exists_ec && !still_exists ) {
            std::cerr << "Deleted old WRF restart file: " << restart_file.path.filename().string() << '\n';
            continue;
        }

        std::cerr << "Warning! Failed to delete old WRF restart file: " << restart_file.path.filename().string() << '\n';
    }

    // ------- Add additional steps here  ----------

    return true;
}

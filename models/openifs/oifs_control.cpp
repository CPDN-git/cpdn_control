//
// Implementation of the OpenIFS model control class.
//  Glenn Carver, CPDN, 2025.

#include "oifs_control.h"
#include "../../lib/utils.h"
#include "oifs_utils.h"    // for oifs_parse_stat()
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_set>

namespace {

constexpr std::string_view OIFS_RCF_FILENAME = "rcf";

ModelControlInputData make_parse_error( const fs::path& source_file, std::string_view error_step, std::string_view error_field,
                                        std::string_view message )
{
    ModelControlInputData result;
    result.source_file = source_file;
    result.error_step = std::string( error_step );
    result.error_field = std::string( error_field );
    result.error_message = std::string( message );
    return result;
}

std::vector<std::string> oifs_get_grib_env_vars( const std::string& slot_path )
{
    return { "GRIB_SAMPLES_PATH=" + slot_path + "/eccodes/ifs_samples/grib1_mlgrib2", "GRIB_DEFINITION_PATH=" + slot_path + "/eccodes/definitions" };
}

std::vector<std::string> oifs_get_omp_env_vars( const std::string& nthreads )
{
    return { "OMP_NUM_THREADS=" + nthreads, "OMP_SCHEDULE=STATIC", "OMP_STACKSIZE=128M" };
}

bool is_ascii_alpha( std::string_view text )
{
    return std::all_of( text.begin(), text.end(), []( unsigned char ch ) { return std::isalpha( ch ) != 0; } );
}

bool is_ascii_digit( std::string_view text )
{
    return std::all_of( text.begin(), text.end(), []( unsigned char ch ) { return std::isdigit( ch ) != 0; } );
}

bool parse_oifs_output_filename( std::string_view filename )
{
    if ( filename.size() != 16 ) {
        return false;
    }

    const std::string_view prefix = filename.substr( 0, 5 );
    if ( prefix != "ICMGG" && prefix != "ICMSH" && prefix != "ICMUA" ) {
        return false;
    }

    if ( !is_ascii_alpha( filename.substr( 5, 4 ) ) ) {
        return false;
    }

    if ( filename[9] != '+' ) {
        return false;
    }

    return is_ascii_digit( filename.substr( 10, 6 ) );
}

}    // namespace


// Implementations of the pure virtual functions from ModelControl

/**
 * @brief Check if the model has completed successfully. Call this after model task has finished.
 * @returns True if the model completed successfully, false otherwise.
 */
bool OpenIFSControl::check_model_success() const
{
    bool success = false;

    // To check whether the model completed successfully, look for 'CNT0' in 3rd column of ifs.stat
    // This will always be the last line of a successful model forecast.

    if ( fs::exists( ifs_stat ) ) {
        std::string ifs_word = "";
        std::string stat_lastline = "";

        fread_last_line( ifs_stat.string(), stat_lastline );    // at some point, these will all be fs::path..
        oifs_parse_stat( stat_lastline, ifs_word, 3 );
        std::cerr << "Last line of ifs.stat, ifs_word: '" << stat_lastline << "', '" << ifs_word << "'\n";
        if ( ifs_word == "CNT0" ) {
            success = true;
        } else {
            std::cerr << "CNT0 not found; string returned was: " << "'" << ifs_word << "'" << '\n';
        }
    } else {
        std::cerr << "ifs.stat file not found: " << ifs_stat << '\n';
    }
    return success;
}


/**
 * @brief Get list of model input files to unpack from the project directory.
 * 
 * @param workunit_id The BOINC workunit ID, used to construct the logical filenames.
 * @return ModelInputManifest List of model input archives with logical names and unzip directories.
 */
ModelInputManifest OpenIFSControl::get_input_manifest( const std::string& workunit_id ) const
{
    // The controller keeps BOINC resolution, checksum verification, copying, and unzip generic.
    // OpenIFS only declares the logical BOINC files it needs and where each archive unpacks.
    return {
        { "ic_ancil_" + workunit_id + ".zip", fs::path( "." ) },
        { "ifsdata_" + workunit_id + ".zip", ifsdata_dir },
        // OpenIFS still uses the climate-data directory named as <res><grid_type>.
        // Rather than use eccodes to read these from the initial GRIB files, unpack to
        // a generic name, then create symlinks to all supported resolutions/grid types.
        // A bit clunky but keeps the model control simpler and avoids adding more args to command line.
        // Note! Relies on call to create directory symlinks in the setup of the model run (see cpdn_control.cpp).
        { "clim_data_" + workunit_id + ".zip", climdata_dir },
    };
}

std::vector<std::string> OpenIFSControl::get_env_vars( const std::string& slot_path, const std::string& nthreads, std::string& err_msg ) const
{
    err_msg.clear();

    if ( std::string nthreads_copy = nthreads; !parse_int( nthreads_copy ) ) {
        err_msg = "invalid value of 'nthreads': " + nthreads;
        return {};
    }

    std::vector<std::string> env_vars = {
        "OIFS_DUMMY_ACTION=abort", "DR_HOOK=1",        "DR_HOOK_HEAPCHECK=no", "DR_HOOK_STACKCHECK=no", "EC_MEMINFO=0",
        "EC_PROFILE_HEAP=0",       "EC_PROFILE_MEM=0",
    };

    auto grib_env_vars = oifs_get_grib_env_vars( slot_path );
    env_vars.insert( env_vars.end(), grib_env_vars.begin(), grib_env_vars.end() );

    auto omp_env_vars = oifs_get_omp_env_vars( nthreads );
    env_vars.insert( env_vars.end(), omp_env_vars.begin(), omp_env_vars.end() );

    return env_vars;
}


ModelControlInputData OpenIFSControl::parse_control_input() const
{
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

        if ( parsed_key == "UTSTEP" ) {
            tmpstr = parsed_value;
            if ( auto decimal_point = tmpstr.find( '.' ); decimal_point != std::string::npos ) {
                tmpstr = tmpstr.substr( 0, decimal_point );
            }
            if ( !parse_int( tmpstr, parsed.timestep_seconds, err_msg ) ) {
                return make_parse_error( control_input_file, "parse", parsed_key, err_msg );
            }
        } else if ( parsed_key == "NFRPOS" ) {
            tmpstr = parsed_value;
            if ( !parse_int( tmpstr, parsed.output_interval, err_msg ) ) {
                return make_parse_error( control_input_file, "parse", parsed_key, err_msg );
            }
        } else if ( parsed_key == "NFRRES" ) {
            tmpstr = parsed_value;
            if ( !parse_int( tmpstr, parsed.restart_interval, err_msg ) ) {
                return make_parse_error( control_input_file, "parse", parsed_key, err_msg );
            }
        } else if ( parsed_key == "CNMEXP" ) {
            parsed.experiment_id = parsed_value;
            if ( parsed.experiment_id.length() != 4 ) {
                return make_parse_error( control_input_file, "validate", parsed_key, "expected a 4-character experiment ID" );
            }
        } else if ( parsed_key == "CUSTOP" ) {
            tmpstr = parsed_value;
            if ( !parse_int( tmpstr, parsed.total_steps, err_msg ) ) {
                return make_parse_error( control_input_file, "parse", parsed_key, err_msg );
            }
        }
    }

    std::vector<std::string> missing_fields;
    if ( parsed.experiment_id.empty() ) {
        missing_fields.push_back( "CNMEXP" );
    }
    if ( parsed.timestep_seconds <= 0 ) {
        missing_fields.push_back( "UTSTEP" );
    }
    if ( parsed.output_interval == 0 ) {
        missing_fields.push_back( "NFRPOS" );
    }
    if ( parsed.restart_interval == 0 ) {
        missing_fields.push_back( "NFRRES" );
    }
    if ( parsed.total_steps <= 0 ) {
        missing_fields.push_back( "CUSTOP" );
    }

    if ( !missing_fields.empty() ) {
        std::string message = "missing or invalid required fields:";
        for ( const auto& field : missing_fields ) {
            message += ' ';
            message += field;
        }
        return make_parse_error( control_input_file, "validate", "", message );
    }

    parsed.forecast_length_time = static_cast<double>( parsed.total_steps ) * static_cast<double>( parsed.timestep_seconds );
    parsed.ok = true;
    return parsed;
}


/**
 * @brief Print the last n lines of key log files produced by the model.
 * @param nlines Number of lines to print from end of each log file.
 */
void OpenIFSControl::print_logs( const int nlines ) const
{
    // TODO: could this be pushed down to the base class rather than re-implemented in each derived class?
    for ( const auto& log_file : log_files ) {
        print_last_lines( log_file, nlines );    // from lib/utils.h; will check file exists
    }
}


/**
 * @brief Get the current model step from the status file.
 * 
 * @param status_file Path to the model status file.
 * @param current_step Reference to an integer to store the current step. Updated on success.
 * @returns True if the current step was successfully retrieved, false otherwise.
 */
bool OpenIFSControl::get_current_step( int& current_step, const int total_steps ) const
{
    std::string lastline{};
    std::string current_step_text;
    std::string err_msg;

    // Read completed step from last line of ifs.stat file.
    // Note the first line from the model has a step count of '....  CNT3      -999 ....'
    // When the iteration number changes in the ifs.stat file, OpenIFS has completed writing
    // to the output files for that iteration, those files can now be moved and uploaded.
    //std::cerr << "Reading completed iteration step from last line of ifs.stat" << std::endl;

    if ( fread_last_line( ifs_stat.string(), lastline ) ) {           // only returns true if lastline is read and changed.
        if ( oifs_parse_stat( lastline, current_step_text, 4 ) ) {    // iter updates
            if ( !parse_int( current_step_text, current_step, err_msg ) ) {
                return false;
            }
            if ( current_step < 0 || current_step > total_steps ) {
                return false;
            }
            return true;
        }
    }
    return false;
}


/**
 * @brief Returns list of model output filenames at a model step.
 *        Used to determine which files to upload at each step.
 * 
 * @param step The model step (string) of files to return.
 * @returns A vector of output filenames to be uploaded.
 */
std::vector<std::string> OpenIFSControl::get_output_filenames( int step ) const
{
    // TODO: exptid should come from the model instance, not via the args
    std::string suffix = oifs_get_filename_part( std::to_string( step ), std::string( exptid ) );
    return { "ICMGG" + suffix, "ICMSH" + suffix, "ICMUA" + suffix };
}

bool OpenIFSControl::is_output_filename( std::string_view filename ) const { return parse_oifs_output_filename( filename ); }

bool OpenIFSControl::is_restart_filename( std::string_view filename ) const { return filename == OIFS_RCF_FILENAME; }

/**
 * @brief Returns vector of list of log files.
 */
std::vector<std::string> OpenIFSControl::get_log_filenames() const { return log_files; }


/** 
 * @brief Returns true if OpenIFS rcf file exists in current dir 
 */
bool OpenIFSControl::restart_ctl_exists() const { return fs::exists( rcf ); }


/**
 * @brief Reads the OpenIFs restart control namelist file "rcf"
 */
bool OpenIFSControl::restart_ctl_read( std::string& step, std::string& time ) const
{
    std::ifstream rcf_stream;
    bool ok = false;

    if ( !fs::exists( rcf ) ) {
        return ok;
    }
    // Read the rcf file
    if ( !( rcf_stream.is_open() ) ) {
        rcf_stream.open( rcf );
    }
    if ( rcf_stream.is_open() ) {
        if ( oifs_read_rcf_file( rcf_stream, time, step ) ) {
            std::cerr << "Read the rcf file" << '\n';
            ok = true;
        }
    }
    rcf_stream.close();

    return ok;
}


/**
 * @brief Sets up any model input directories and/or symlinks as needed before staging the input files.
 * For OpenIFS, we need to create symlinks to the climdata directory for all supported resolutions/grid types.
 * This is because the model expects a specific directory structure for the climate data input which includes the
 * resolution and grid type in the path. To avoid having to add more args to the command line or read these from 
 * the control input, we unpack to a generic name, then create symlinks to all supported resolutions/grid types.
 * 
 * @param slot_path The path to the model run slot directory where the input files are staged and the model runs.
 * @returns True if the directories and symlinks were set up successfully, false otherwise.
 */
bool OpenIFSControl::setup_directories( const fs::path& slot_path ) const
{
    std::string err_msg;

    // List of supported resolutions and grid types for the model. horiz res + grid type.
    std::vector<std::string> supported_resolutions = { "95_4", "159l_2", "159_4", "199_4", "255l_2", "319l_2", "319_4", "399l_2", "511l_2" };

    if ( !ensure_directory( climdata_dir, &err_msg ) ) {
        std::cerr << "Climate data directory not found at expected path: " << climdata_dir << '\n';
        std::cerr << "Error message: " << err_msg << '\n';
        return false;
    }

    for ( const auto& res : supported_resolutions ) {
        fs::path link_name = slot_path / res;
        if ( fs::exists( link_name ) ) {
            continue;    // if the link already exists, skip creating it
        }
        try {
            fs::create_symlink( climdata_dir, link_name );
            std::cerr << "Created symlink: " << link_name << " -> " << climdata_dir << '\n';
        } catch ( const fs::filesystem_error& e ) {
            std::cerr << "Error creating symlink: " << e.what() << '\n';
            return false;
        }
    }
    return true;
}

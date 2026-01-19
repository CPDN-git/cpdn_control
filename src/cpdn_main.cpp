//
// BOINC task controller for CPDN.
//
// This version written by Glenn Carver, CPDN, 2025.
// Original version by Andy Bowery (Oxford eResearch Centre, Oxford University) December 2023.
//

#include <chrono>
#include <cstdlib>
#include <dirent.h>    // this and...
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unordered_set>
#include <vector>

#include "boinc/boinc_api.h"

#include "cpdn_control.h"
#include "cpdn_zip.h"
#include "lib/cpdn_cpu_time.h"
#include "lib/utils.h"

#include "api/model_control.h"
#include "api/progressfile_handler.h"
#include "api/trickle_handler.h"

#include "models/openifs/oifs_control.h"

// these includes will disappear when the code moves to Model derived classes
#include "models/openifs/oifs_utils.h"    // for oifs_get_filename_part, oifs_*() functions.


namespace chrono = std::chrono;
namespace fs = std::filesystem;


// Define the code version if not defined at compile time with -D option.
#ifndef CODE_VERSION
#define CODE_VERSION "1.0.0"
#endif


// Constants
constexpr std::string_view MODEL_CONFIG_FILE = "model_config.xml";


// ------------------------------------------
// --------------- Functions ----------------

/**
 * @brief Factory function to create ModelControl instance based on model name.
 *        Note that we specify the *model* name here and not the app name though they may be the same.
 * 
 * @param modelName The name of the model.
 * @return A unique pointer to the created ModelControl instance. Maybe nullptr if model not supported.
*/
static std::unique_ptr<ModelControl> create_model_control( std::string_view vendor, std::string_view model_name, std::string_view model_version,
                                                           std::string_view primary_ctrl_file )
{
    std::unique_ptr<ModelControl> model;    // create a null unique_ptr ready for a new model control instance.

    // Model mappings
    // As the test model is an OpenIFS skeleton clone, we use the OpenIFSControl class.

    if ( model_name == "test_model" || model_name == "oifs_43r3" ) {
        model = std::make_unique<OpenIFSControl>( vendor, model_name, model_version, primary_ctrl_file );
    }

    return model;
}


/**
 * @brief Parse and validate the --nthreads argument from app_config.xml.
 * 
 * @param app_config_nthreads The string value of the nthreads argument.
 * @param nthreads Altered number of threads as a string.
 * @param err_msg Error string if parsing fails.
 * @returns True if the nthreads argument was valid and changed, false otherwise.
 */
static bool get_app_config_nthreads( const std::string& app_config_nthreads, std::string& nthreads, std::string& err_msg )
{
    err_msg.clear();
    if ( app_config_nthreads.empty() ) {
        std::cerr << "Warning. --nthreads argument present but has no value! Ignoring.\n";
    } else {
        // GC. The best max as parallel efficiency markedly drops after this many threads, even at T319.
        int max_threads = 8;
        int min_threads = 1;    // minimum number of threads.
        int ithreads = -1;

        std::string nthreads_value = app_config_nthreads;
        if ( !parse_int( nthreads_value, ithreads, err_msg ) ) {
            std::cerr << "Warning. --nthreads argument must be a valid integer! " << err_msg << '\n';
            return false;
        }

        if ( ithreads > max_threads ) {
            std::cerr << "Warning. --nthreads value too high. Setting to max number of threads : " << max_threads << '\n';
            nthreads = std::to_string( max_threads );
        } else if ( ithreads < min_threads ) {
            std::cerr << "Warning. --nthreads is too low for this configuration. Minimum #threads is 2. Resetting.\n";
            nthreads = std::to_string( min_threads );
        }
        return true;
    }
    return false;
}


/**
 * @brief Construct the result base name for result files.
 *        When running under BOINC, this comes from the resolved part
 *        of the first upload file. When running standalone, we make
 *        up a reasonable name based on the workunit parameters.
 * @return Result base name string (without path or .zip)
 */
static std::string get_result_base_name( const BoincConfig& bconfig, const TaskConfig& tconfig )
{
    std::string base_name;

    if ( !bconfig.standalone ) {
        std::string resolved_name;
        int retval = boinc_resolve_filename_s( "upload_file_0.zip", resolved_name );
        if ( retval ) {
            std::cerr << "..boinc_resolve_filename failed" << std::endl;
            return base_name;
        }

        base_name = fs::path( resolved_name ).stem();    // returns filename without path nor '.zip'
        if ( base_name.length() > 2 ) {
            base_name.erase( base_name.length() - 2 );    // remove the '_0'
        }
        if ( base_name.compare( "upload_file" ) == 0 ) {
            std::cerr << "..Failed to get result name" << std::endl;
            return base_name;
        }
    } else {
        base_name = bconfig.app_name + "_" + tconfig.start_date + "_" + tconfig.batchid + "_" + tconfig.wuid;
    }
    return base_name;
}

/**
 * @brief Append upload files that match the expected output filename pattern.
 *
 * @returns zero on success, otherwise error code value.
 */
static int add_upload_files( const fs::path& dir, std::vector<fs::path>& out, const std::regex& pattern )
{
    std::error_code ec;

    for ( const auto& entry : fs::directory_iterator( dir, ec ) ) {
        if ( ec ) {
            std::cerr << "..Unable to scan upload directory: " << dir << " (" << ec.message() << ")\n";
            return ec.value();
        }
        if ( !entry.is_regular_file( ec ) ) {
            if ( ec ) {
                std::cerr << "..Unable to read directory entry: " << entry.path() << " (" << ec.message() << ")\n";
                return ec.value();
            }
            continue;
        }

        const auto filename = entry.path().filename().string();
        if ( std::regex_match( filename, pattern ) ) {
            out.push_back( entry.path() );
            std::cerr << "Adding to the zip: " << entry.path().string() << '\n';
        }
    }
    return 0;
}

/**
 * @brief Process command line arguments to populate TaskConfig.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @param tconfig TaskConfig structure to populate.
 * @return 0 on success, non-zero on failure.
 */
static int process_args( int argc, char** argv, TaskConfig& tconfig )
{
    if ( argc < 7 ) {
        std::cerr << "CPDN Controller error: Not enough command line arguments provided.\n"
                  << "Usage: " << argv[0] << " <start_date> <exptid> <unique_member_id> <batchid> <wuid> <fclen> [app_version]\n";
        return 1;
    }

    // Read the exptid, umid, batchid, wuid, fclen from the command line
    tconfig.start_date = argv[1];          // simulation start date
    tconfig.exptid = argv[2];              // OpenIFS experiment id
    tconfig.unique_member_id = argv[3];    // umid
    tconfig.batchid = argv[4];             // batch id
    tconfig.wuid = argv[5];                // workunit id
    tconfig.fclen = argv[6];               // number of simulation days

    return 0;
}


/**
 * @brief Prints a banner to stderr at start of controller with model name and version.
 */
static void banner( const std::string& model_name, const std::string& model_version, const std::string& code_version )
{
    fprintf( stderr, "\n\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n" );
    fprintf( stderr, "|  CPDN task controller starting: version %s \n", code_version.c_str() );
    fprintf( stderr, "|  Model name: %s. Model version: %s \n", model_name.c_str(), model_version.c_str() );
    fprintf( stderr, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n\n" );
}


/**
 * @brief Cleanup and finish the task. 
 *        End the task by calling boinc_finish and returning the same exit code.
 *        boinc_finish exits under BOINC, but return is kept for dummy libraries.
 */
static int task_finish( int exit_code )
{
    // Add in any task cleanup code here as needed.

    boinc_end_critical_section();    // in case we abort while in critical section (boinc api handles case if not in critical section).
    boinc_finish( exit_code );       // boinc_finish exits, no further code executed after this call (unless a dummy library is used).
    return exit_code;
}


//----------------------------------------------------------------------------
//----------------------------- MAIN PROGRAM ---------------------------------


/**
 * @brief Main program for the CPDN task controller.
 */
int main( int argc, char** argv )
{
    BoincConfig bconfig;    // BOINC settings from init_data.xml.
    TaskConfig tconfig;     // CPDN task settings from command line.
    int retval = 0;
    std::string err_msg;

    // ------------- BOINC Initialisation -----------------

    // Initialise BOINC to get the project directory, workunit name and app version
    // Note this redirects stderr output to stderr.txt in slot dir.
    retval = init_boinc( bconfig );
    if ( retval ) {
        std::cerr << "..BOINC initialisation failed" << "\n";
        return retval;
    }
    if ( bconfig.slot_path.empty() ) {
        std::cerr << "..Error. Can't determine slot path: current_path() returned empty" << std::endl;
        return task_finish( 1 );
    }
    std::cerr << "Working slot directory is: " << bconfig.slot_path << '\n';
    std::cerr << "Project directory is: " << bconfig.project_dir << '\n';
    std::cerr << "Running in standalone mode" << '\n';

    // Say who we are.
    banner( bconfig.app_name, bconfig.app_version, CODE_VERSION );
    std::cerr << "Workunit name: " << bconfig.wu_name << '\n' << "CPDN project directory: " << bconfig.project_dir << '\n';

    // ---------------- Task configuration -----------------

    // TODO. Read in the model config.xml.  The XML file contains all the information
    // about the model. It's required to initialize the correct model class later on.

    // Check for existence of model_config.xml in current directory (task) and fail if not found.
    if ( !path_exists( MODEL_CONFIG_FILE ) ) {
        std::cerr << "..The model config.xml file does not exist in the current directory: " << MODEL_CONFIG_FILE << std::endl;
        //GC. Testing only; return task_finish(1);        // should terminate, the model won't run.
    }

    // Create model control instance.
    // In future, rather than pass app_name, we might pass the model name read from model_config.xml.
    // "CPDN" and "fort.4" are placeholders for vendor name and primary control file respectively.
    auto model_ctrl = create_model_control( "ECMWF", bconfig.app_name, bconfig.app_version, "fort.4" );
    if ( model_ctrl == nullptr ) {
        std::cerr << "..Error creating model control instance. Unsupported model: " << bconfig.app_name << std::endl;
        return task_finish( 1 );
    }

    // --------------- Argument processing -----------------

    // app_name & nthreads have been removed from command line args, they now come from BOINC init_data.xml.
    retval = process_args( argc, argv, tconfig );
    if ( retval ) {
        std::cerr << "..Error processing command line arguments" << std::endl;
        return task_finish( retval );
    }

    // Check for optional '--nthreads <value>' at end of arg list optionally set by app_config.xml on user's machine.
    // TODO: look at removing string copy of nthreads and use int bconfig.ncpus throughout code. DRY.

    std::string nthreads = std::to_string( bconfig.ncpus );    // default number of threads from BOINC init_data.xml
    int nthreads_int = bconfig.ncpus;
    if ( std::string( argv[argc - 2] ) == "--nthreads" ) {
        std::string app_config_nthreads = argv[argc - 1];
        if ( !get_app_config_nthreads( app_config_nthreads, nthreads, err_msg ) && !err_msg.empty() ) {
            std::cerr << "..Failed to parse --nthreads argument: " << err_msg << '\n';
            return task_finish( 1 );
        }
        std::string nthreads_value = nthreads;
        if ( !parse_int( nthreads_value, nthreads_int, err_msg ) ) {
            std::cerr << "..Failed to parse --nthreads value: " << err_msg << '\n';
            return task_finish( 1 );
        }
        bconfig.ncpus = nthreads_int;
        std::cerr << "Using --nthreads from app_config.xml: " << nthreads << '\n';
    }

    const std::string namelist = "fort.4";              // namelist file. will come from XML input later.
    double num_days = atof( tconfig.fclen.c_str() );    // number of simulation days; fclen should come from fort.4, not the command line.

    // --------------- Prepare the task environment -----------------

    boinc_begin_critical_section();

    // Create temp upload folder for moving the results to and uploading the results from.
    // BOINC measures the disk usage on the slots directory so we must move all results out of this folder
    std::string upload_dir = bconfig.project_dir + bconfig.app_name + "_" + tconfig.wuid;
    std::cerr << "Location of temp folder: " << upload_dir << '\n';
    if ( !path_exists( upload_dir ) ) {
        if ( mkdir( upload_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH ) != 0 ) {
            std::cerr << "..mkdir for temp folder for results failed" << std::endl;
            return task_finish( 1 );    // should terminate, the model won't run.
        }
    }

    //  Unpack application into slot
    retval = move_and_unzip_app_file( bconfig.app_name, bconfig.app_version, bconfig.project_dir, bconfig.slot_path );
    if ( retval ) {
        std::cerr << "..move_and_unzip_app_file failed" << "\n";
        return task_finish( retval );
    }

    //------------------------------------------Process the namelist-----------------------------------------
    // GC. Note, this is not the 'model fort.4' namelist file being referred to here. Needs renaming to avoid confusion.
    // This should really be part of a general piece of code to process the model ancil files. Needs refactoring later.

    fs::path namelist_zip_path = bconfig.slot_path;
    namelist_zip_path /= std::string( bconfig.app_name ) + "_" + tconfig.unique_member_id + "_" + tconfig.start_date + "_" +
                         std::to_string( (int)num_days ) + "_" + tconfig.batchid + "_" + tconfig.wuid + ".zip";
    std::string namelist_zip = namelist_zip_path.string();    // nb this is a const string.

    // Copy the namelist_zip to the slot directory and unzip
    if ( copy_and_unzip( namelist_zip, namelist_zip, bconfig.slot_path, "namelist_zip" ) ) {
        std::cerr << "..Copying and unzipping the namelist_zip failed: " << namelist_zip << std::endl;
        return task_finish( 1 );    // should terminate, the model won't run.
    }

    // Parse the fort.4 namelist for the filenames and variables
    std::string ifsdata_file;
    std::string ic_ancil_file;
    std::string climate_data_file;
    std::string namelist_file = bconfig.slot_path + "/" + namelist;
    std::string namelist_line;
    std::string horiz_resolution;
    std::string vert_resolution;
    std::string grid_type;
    std::string tmpstr;

    std::ifstream namelist_filestream;

    int upload_interval = 0;
    int trickle_freq = 0;
    int timestep = 0;
    int ICM_file_interval = 0;
    int restart_interval = 0;

    // Check for the existence of the namelist
    if ( !path_exists( namelist_file ) ) {
        std::cerr << "..The namelist file does not exist: " << namelist_file << std::endl;
        return task_finish( 1 );    // should terminate, the model won't run.
    }

    // Read the model's controlling namelist file

    if ( !( namelist_filestream.is_open() ) ) {
        namelist_filestream.open( namelist_file );
    }
    if ( !namelist_filestream.is_open() ) {
        std::cerr << "..Error opening namelist file: " << namelist_file << std::endl;
        return task_finish( 1 );
    }

    std::string parsed_key;
    std::string parsed_value;

    // Parsing the namelist file at the moment is a mix of looking for CPDN injected
    // header variables and normal model namelist variables. It's a bit clumsy but works for now.
    // It's not my code and I'm tidying the data flow to be more consistent.

    // The header_keys are the CPDN injected task related parameters always at the
    // top of the namelist file. It's not a tidy solution as they are prefixed with
    // '!' making them normal format comments. This causes issues parsing them as if we
    // allow commented key/value pairs to be parsed, the code picks up other commented
    // out namelist variables which has caused errors. Ideally it would have been
    // better to prefix with something unique such as !TASK but as this involves
    // changing the oifs_workgen repo and I plan to tidy this whole area up by
    // reducing the usage of header parameters like this, for now we will simply
    // look for these specific keys only in the header block and eliminate them
    // as they become redundant.
    // The rather hacky code below assumes reading header variables until it hits the
    // first true namelist.
    //    Glenn   Jan 2026.

    bool in_header = true;    // goes false when we reach the first namelist '&' line.

    // These are the keys injected by CPDN into the namelist header. Other variables
    // searched for come from the namelist itself.
    const std::unordered_set<std::string> header_keys = { "IC_ANCIL_FILE",   "IFSDATA_FILE", "CLIMATE_DATA_FILE", "HORIZ_RESOLUTION",
                                                          "VERT_RESOLUTION", "GRID_TYPE",    "UPLOAD_INTERVAL" };

    while ( std::getline( namelist_filestream, namelist_line ) ) {
        tmpstr.clear();
        parsed_key.clear();
        parsed_value.clear();

        trim_whitespace( namelist_line );
        if ( namelist_line.empty() ) {
            continue;
        }

        bool have_kv = false;
        std::string header_line = namelist_line;

        if ( in_header ) {
            if ( header_line.front() == '&' ) {
                in_header = false;
                continue;
            }
            if ( header_line.front() == '!' ) {    // possible header key/value pair
                header_line.erase( 0, 1 );

                if ( parse_key_value( header_line, parsed_key, parsed_value ) &&
                     header_keys.find( parsed_key ) != header_keys.end() ) {    // ignore any keys not in the header list above
                    have_kv = true;
                }
            }
        } else {    // normal namelist parsing
            if ( !parse_namelist_key_value( namelist_line, parsed_key, parsed_value ) ) {
                continue;
            }
            have_kv = true;
        }

        if ( !have_kv ) {    // skip lines that didn't yield a key/value pair
            continue;
        }

        if ( parsed_key == "IFSDATA_FILE" ) {
            ifsdata_file = parsed_value;
        } else if ( parsed_key == "IC_ANCIL_FILE" ) {
            ic_ancil_file = parsed_value;
        } else if ( parsed_key == "CLIMATE_DATA_FILE" ) {
            climate_data_file = parsed_value;
        } else if ( parsed_key == "HORIZ_RESOLUTION" ) {
            horiz_resolution = parsed_value;
        } else if ( parsed_key == "VERT_RESOLUTION" ) {
            vert_resolution = parsed_value;
        } else if ( parsed_key == "GRID_TYPE" ) {
            grid_type = parsed_value;
        } else if ( parsed_key == "UPLOAD_INTERVAL" ) {
            tmpstr = parsed_value;
            if ( !parse_int( tmpstr, upload_interval, err_msg ) ) {
                std::cerr << "..Failed to parse upload interval from namelist: " << err_msg << '\n';
                return task_finish( 1 );
            }
        } else if ( parsed_key == "UTSTEP" ) {
            // UTSTEP (secs) is written as a float in the namelist, despite it only ever being an integer.
            // parse_int is strict about parsing only integer representations.
            tmpstr = parsed_value;
            if ( auto dp = tmpstr.find( '.' ); dp != std::string::npos ) {
                tmpstr = tmpstr.substr( 0, dp );
            }
            if ( !parse_int( tmpstr, timestep, err_msg ) ) {
                std::cerr << "..Failed to parse timestep interval from namelist: " << err_msg << '\n';
                return task_finish( 1 );
            }
        } else if ( parsed_key == "NFRPOS" ) {    // frequency of model OUTPUT file creation (for upload); +ve model steps, -ve hours.
            tmpstr = parsed_value;
            if ( !parse_int( tmpstr, ICM_file_interval, err_msg ) ) {
                std::cerr << "..Failed to parse ICM model output interval from namelist: " << err_msg << '\n';
                return task_finish( 1 );
            }
        } else if ( parsed_key == "NFRRES" ) {    // frequency of model RESTART file creation: +ve model steps, -ve hours.
            tmpstr = parsed_value;
            if ( !parse_int( tmpstr, restart_interval, err_msg ) ) {
                std::cerr << "..Failed to parse restart interval from namelist: " << err_msg << '\n';
                return task_finish( 1 );
            }
        }
    }
    namelist_filestream.close();

    // Check for any empty variables in case parsing failed.
    // These might cause the task to fail later, or they might be deliberate for testing.
    if ( ifsdata_file.empty() )
        std::cerr << ".. Warning. Unable to parse ifs_data_file from namelist.\n";
    if ( ic_ancil_file.empty() )
        std::cerr << ".. Warning. Unable to parse ic_ancil_file from namelist.\n";
    if ( climate_data_file.empty() )
        std::cerr << ".. Warning. Unable to parse climate_data_file from namelist.\n";
    if ( horiz_resolution.empty() )
        std::cerr << ".. Warning. Unable to parse horiz_resolution from namelist.\n";
    if ( vert_resolution.empty() )
        std::cerr << ".. Warning. Unable to parse vert_resolution from namelist.\n";
    if ( grid_type.empty() )
        std::cerr << ".. Warning. Unable to parse grid_type from namelist.\n";

    std::cerr << "Values read from model namelist are: \n"
              << " ifsdata_file: " << ifsdata_file << '\n'
              << " ic_ancil_file: " << ic_ancil_file << '\n'
              << " climate_data_file: " << climate_data_file << '\n'
              << " horiz_resolution: " << horiz_resolution << '\n'
              << " vert_resolution: " << vert_resolution << '\n'
              << " grid_type: " << grid_type << '\n'
              << " Upload_interval: " << upload_interval << '\n'
              << " UTSTEP (timestep interval): " << timestep << '\n'
              << " NFRPOS (frequency of model output): " << ICM_file_interval << '\n'
              << " NFFRES (frequency of restarts/checkpoints): " << restart_interval << std::endl;


    //   Secondary run parameters

    // restart frequency might be in units of hrs, convert to model steps
    if ( restart_interval < 0 ) {
        restart_interval = abs( restart_interval ) * 3600 / timestep;
        std::cerr << " NFRRES: restart dump frequency (in steps) " << restart_interval << '\n';
    }

    // this should match CUSTOP in fort.4. If it doesn't we have a problem.
    double total_nsteps = ( num_days * 86400.0 ) / (double)timestep;    //GC. why is this a double? it's always an int.

    //GC. Oct/25. Trickles are now fixed at every 10% of the model run with a final trickle at the end of the run.
    //    Value read from fort.4 namelist is ignored and should be removed.
    trickle_freq = TrickleHandler::get_trickle_frequency( timestep, (int)total_nsteps );

    std::cerr << "Trickle frequency is every 10% of model run : " << trickle_freq << " model steps, "
              << ( (float)trickle_freq * (float)timestep ) / 86400.0 << " days.\n";

    //-------------------------------------------------------------------------------------------------------
    //    Unpack the task's input files into the slot directory

    // Process the ic_ancil_file:
    std::string ic_ancil_zip = bconfig.slot_path + "/" + ic_ancil_file + ".zip";

    // Copy the ic_ancil_zip to the slot directory and unzip
    if ( copy_and_unzip( ic_ancil_zip, ic_ancil_zip, bconfig.slot_path, "ic_ancil_zip" ) ) {
        std::cerr << "..Copying and unzipping the ic_ancil_zip failed: " << ic_ancil_zip << std::endl;
        return task_finish( 1 );    // should terminate, the model won't run.
    }

    // Process the ifsdata_file:
    // Make the ifsdata directory and set the required paths
    std::string ifsdata_folder = bconfig.slot_path + "/ifsdata";
    std::string ifsdata_zip = bconfig.slot_path + "/" + ifsdata_file + ".zip";
    std::string ifsdata_destination = ifsdata_folder + "/" + ifsdata_file + ".zip";

    // Check if ifsdata folder does not already exists or is empty
    if ( !path_exists( ifsdata_folder ) ) {
        if ( mkdir( ifsdata_folder.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH ) != 0 ) {
            std::cerr << "..mkdir for ifsdata folder failed" << std::endl;
            return task_finish( 1 );    // should terminate, the model won't run.
        }
    }

    // Copy the ifsdata_zip to the slot directory and unzip
    // GC TODO. convert to fs::path and get rid of handling '/'
    std::string ifsdata_check = ifsdata_folder + "/";
    if ( copy_and_unzip( ifsdata_zip, ifsdata_destination, ifsdata_check, "ifsdata_zip" ) ) {
        std::cerr << "..Copying and unzipping the ifsdata_zip failed: " << ifsdata_zip << std::endl;
        return task_finish( 1 );    // should terminate, the model won't run.
    }

    // Process the climate_data_file:
    // Make the climate data directory and set the required paths
    std::string climate_data_path = bconfig.slot_path + "/" + horiz_resolution + grid_type;
    std::string climate_data_zip = bconfig.slot_path + "/" + climate_data_file + ".zip";
    std::string climate_data_destination = climate_data_path + "/" + climate_data_file + ".zip";

    // Check if climate_data folder does not already exists or is empty
    if ( !path_exists( climate_data_path ) ) {
        if ( mkdir( climate_data_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH ) != 0 ) {
            std::cerr << "..mkdir for the climate data folder failed" << std::endl;
            return task_finish( 1 );
        }
    }

    // Copy the climate_data_zip to the slot directory and unzip
    if ( copy_and_unzip( climate_data_zip, climate_data_destination, climate_data_path, "climate_data_zip" ) ) {
        std::cerr << "..Copying and unzipping the climate_data_zip failed: " << climate_data_zip << std::endl;
        return task_finish( 1 );    // should terminate, the model won't run.
    }

    //-------------------------------------------------------------------------------------------------------

    // Initialize task state with default value
    TaskState tstate;

    // Initialise the ProgressFile handler
    ProgressFileHandler progress_file( bconfig.slot_path );

    // Define the location of the OpenIFS rcf file
    std::string rcf_file = bconfig.slot_path + "/rcf";

    // Check whether the rcf file and the progress file (contains model progress) are not already present from an unscheduled shutdown
    std::cerr << "Checking for rcf file and progress file: " << progress_file.path() << '\n';

    // Handle the cases of the various states of the rcf file and progress file.
    if ( !progress_file.exists() && !path_exists( rcf_file ) ) {

        // Both progress file and rcf file do not exist, model has not run.
        // Do nothing as the task state variables are already initialized to zero values above.
        std::cerr << "-- Starting new model run --\n";

    } else if ( ( progress_file.exists() && !progress_file.is_empty() ) && path_exists( rcf_file ) ) {

        // If progress file exists, not empty and rcf file exists, then this is a restart.
        // Check the rcf file against the progress file and continue the run.
        std::ifstream rcf_file_stream;
        std::string cstep_value;

        // Read the rcf file
        if ( path_exists( rcf_file ) ) {
            if ( !( rcf_file_stream.is_open() ) ) {
                rcf_file_stream.open( rcf_file );
            }
            if ( rcf_file_stream.is_open() ) {
                std::string ctime_value;
                if ( oifs_read_rcf_file( rcf_file_stream, ctime_value, cstep_value ) ) {
                    std::cerr << "Read the rcf file" << '\n';
                } else {
                    // Reading the rcf file failed, then kill model run
                    model_ctrl->print_logs( 50 );
                    std::cerr << "..Reading the rcf file failed" << '\n';
                    return task_finish( 1 );
                }
            }
        }
        rcf_file_stream.close();

        if ( !progress_file.read( tstate, err_msg ) ) {
            std::cerr << "..Failed to read progress file: " << err_msg << '\n';
            return task_finish( 1 );
        }

        int cstep_int = 0;
        std::string cstep_str = cstep_value;
        if ( !parse_int( cstep_str, cstep_int, err_msg ) ) {
            std::cerr << "..Failed to parse CSTEP value from rcf: " << err_msg << '\n';
            return task_finish( 1 );
        }

        int last_step_int = 0;
        std::string last_step_str = tstate.last_step;
        if ( !parse_int( last_step_str, last_step_int, err_msg ) ) {
            std::cerr << "..Failed to parse last_step from progress file: " << err_msg << '\n';
            return task_finish( 1 );
        }

        // Check if the CSTEP variable from rcf is greater than the last_step, if so then quit model run
        // This is probably recoverable, but it might mean the model ran on after the controller crashed, so end for now.
        if ( cstep_int > last_step_int ) {
            std::cerr << "..CSTEP variable from rcf greater than last_step from progress file, error has occurred, quitting model run" << '\n';
            return task_finish( 1 );
        }

        // Adjust last_step to the step of the previous model restart dump step.
        // This is always a multiple of the restart frequency

        std::cerr << "-- Model is restarting --\n";
        std::cerr << "Adjusting last_step, " << tstate.last_step << ", to previous model restart step.\n";
        int restart_step = last_step_int;
        restart_step = restart_step - ( ( restart_step % restart_interval ) - 1 );    // -1 because the model will continue from restart_step.
        tstate.last_step = std::to_string( restart_step );

    } else if ( progress_file.exists() && file_is_empty( progress_file.path() ) ) {

        // If progress file exists and is empty, an error has occurred, then kill model run
        // GC. TODO. Review this. It might mean the we didn't get to the point where the progress file was written.?
        model_ctrl->print_logs( 50 );
        std::cerr << "..progress file exists, but is empty => problem with model, quitting run" << '\n';
        return task_finish( 1 );

    } else if ( progress_file.exists() && !path_exists( rcf_file ) ) {

        // GC. TODO. I think this needs merging with case above of restart from existing rcf file?
        // If the progress file exists, the model has started but not yet got to the first
        // restart write. In which case the model starts from the beginning again.
        if ( !progress_file.read( tstate, err_msg ) ) {
            std::cerr << "..Failed to read progress file: " << err_msg << '\n';
            return task_finish( 1 );
        }
        // If last_step less than restart interval, model rcf has yet to be produced so model will restart from beginning.
        int last_step_int = 0;
        std::string last_step_str = tstate.last_step;
        if ( !parse_int( last_step_str, last_step_int, err_msg ) ) {
            std::cerr << "..Failed to parse last_step from progress file: " << err_msg << '\n';
            return task_finish( 1 );
        }
        if ( last_step_int >= restart_interval ) {
            // Otherwise if progress file exists and rcf file does not exist, an error has occurred, then kill model run
            model_ctrl->print_logs( 50 );
            std::cerr << "..progress file exists, but rcf file does not exist => problem with model, quitting run" << '\n';
            return task_finish( 1 );
        }

    } else if ( !progress_file.exists() && path_exists( rcf_file ) ) {
        // If rcf file exists and progress file does not exist, an error has occurred, then kill model run
        // TODO: we should be able to bootstrap the progress file from the rcf file here?
        // Maybe not as the model likely runs on after the controller process has crashed.
        model_ctrl->print_logs( 50 );
        std::cerr << "..rcf file exists, but progress file does not exist => problem with task, quitting run" << '\n';
        return task_finish( 1 );
    }

    // Update progress file with current values
    if ( !progress_file.write( tstate, err_msg ) ) {
        std::cerr << "..Failed to write progress file: " << err_msg << '\n';
        return task_finish( 1 );
    }

    // seconds between upload files: upload_interval
    // seconds between ICM files: ICM_file_interval * timestep
    // upload interval in steps = upload_interval / timestep
    //cerr "upload_interval: "<< upload_interval << ", timestep: " << timestep << '\n';

    // Check if upload_interval x timestep equal to zero
    if ( upload_interval * timestep == 0 ) {
        std::cerr << "..upload_interval x timestep equals zero" << std::endl;
        return task_finish( 1 );
    }

    auto total_length_of_simulation = (int)( num_days * 86400 );
    std::cerr << "Total_length_of_simulation: " << total_length_of_simulation << '\n';

    // Get result_base_name to construct upload file names for both standalone and under BOINC.

    std::string result_base_name = get_result_base_name( bconfig, tconfig );
    std::cerr << "result_base_name: " << result_base_name << '\n';

    // Create the trickle handler (only trickle if not in standalone mode)
    TrickleHandler trickler( bconfig.wu_name, result_base_name, bconfig.slot_path );

    // Determine which OpenIFS executable to run.
    // GC. This should be an input parameter on the command line or the init_data.xml (or model_config.xml) later on.

    fs::path single_proc_exe = bconfig.slot_path;
    single_proc_exe /= "oifs_43r3_model.exe";

    fs::path multi_proc_exe = bconfig.slot_path;
    multi_proc_exe /= "oifs_43r3_omp_model.exe";

    fs::path test_proc_exe = bconfig.slot_path;
    test_proc_exe /= "test_model";

    std::string exe_cmd{};

    // GC TODO this needs tidying up; exec name should come from model class.
    if ( path_exists( single_proc_exe.string() ) ) {
        exe_cmd = single_proc_exe.string();
    } else if ( path_exists( multi_proc_exe.string() ) ) {
        exe_cmd = multi_proc_exe.string();
    } else if ( path_exists( test_proc_exe.string() ) ) {
        exe_cmd = test_proc_exe.string();
    }
    if ( exe_cmd.empty() ) {
        std::cerr << "..No model executable found, ending task." << std::endl;
        return task_finish( 1 );
    }

    // Bug workaround. The current cpdn_unzip function does not preserve executable permissions on Linux.
    // Manually set the permissions on the model executable before running.
    // GC. Dec/2025

    if ( !set_exec_perms( exe_cmd ) ) {
        std::cerr << "..Cannot start model. Setting execute permission for model executable failed: " << exe_cmd << std::endl;
        return task_finish( 1 );
    }

    // Start the model process
    std::cerr << "Launching model executable: " << exe_cmd << std::endl;
    tstate.pid = launch_process( bconfig.project_dir, bconfig.slot_path, exe_cmd, nthreads, tconfig.exptid );

    if ( tstate.pid > 0 ) {
        tstate.process_status = 0;
    } else if ( tstate.pid == -1 ) {
        std::cerr << "..Error launching model process, return value: " << tstate.pid << std::endl;
        return task_finish( 1 );
    }

    boinc_end_critical_section();


    // process_status = 0 running
    // process_status = 1 stopped normally
    // process_status = 2 stopped with quit request from BOINC
    // process_status = 3 stopped with child process being killed
    // process_status = 4 stopped with child process being stopped
    // process_status = 5 child process not found by waitpid()


    //----------------------------------------Main loop------------------------------------------------------

    // Periodically check the process status and the BOINC client status
    std::string ifs_stat = bconfig.slot_path + "/ifs.stat";    // GC. TODO: should be std::filesystem path.

    std::vector<fs::path> zfl;

    int delay_count = 0;
    std::string step = "0";

    while ( tstate.process_status == 0 && tstate.model_completed == 0 ) {
        sleep_seconds( 1 );    // Time delay to reduce overhead

        delay_count++;

        // Check whether an upload point has been reached
        // GC. 09/25. reduced to 7 secs as testing shows 10secs can miss a timestep.
        // Going too low can cause the %age done on boincmgr to flip backwards.
        if ( delay_count == 7 ) {

            // Get the current model step.
            step = tstate.last_step;
            if ( path_exists( ifs_stat ) ) {
                // step is updated by this call if successful.
                if ( !model_ctrl->get_current_step( ifs_stat, step, total_nsteps ) ) {
                    step = tstate.last_step;    // revert to last valid step
                }
            }

            int step_value = 0;
            std::string step_str = step;
            if ( !parse_int( step_str, step_value, err_msg ) ) {
                std::cerr << "..Failed to parse current step: " << err_msg << '\n';
                return task_finish( 1 );
            }

            int last_step_value = 0;
            std::string last_step_str = tstate.last_step;
            if ( !parse_int( last_step_str, last_step_value, err_msg ) ) {
                std::cerr << "..Failed to parse last_step: " << err_msg << '\n';
                return task_finish( 1 );
            }

            // Move the model result files to the task folder in the project directory
            // GC. Why do this every timestep? This check only needs to be done at same frequency as NFRPOS.
            if ( step_value != last_step_value ) {

                for ( const auto& result : model_ctrl->get_output_filenames( tstate.last_step, tconfig.exptid ) ) {
                    retval = move_result_file( bconfig.slot_path, upload_dir, result );
                    if ( retval ) {
                        std::cerr << "..Copying " << result << " result file to the temp folder in the projects directory failed" << "\n";
                        return task_finish( retval );
                    }
                }

                // Convert current model step to seconds
                tstate.current_step = last_step_value * timestep;

                // Upload a new upload file if the end of an upload_interval has been reached
                // GC. TODO. Why not combine adding to the zip file with moving the result files above?
                if ( ( ( tstate.current_step - tstate.last_upload ) >= ( upload_interval * timestep ) ) &&
                     ( tstate.current_step < total_length_of_simulation ) ) {
                    // Create an intermediate results zip file
                    zfl.clear();

                    std::cerr << "End of upload interval reached, starting a new upload process" << std::endl;

                    // *****  Critical section start  *****
                    boinc_begin_critical_section();

                    // Cycle through all the steps from the last upload to the current upload
                    //  GC. tstate.current_step/timestep is just tstate.last_step! Fix!
                    for ( auto i = ( tstate.last_upload / timestep ); i < ( tstate.current_step / timestep ); i++ ) {

                        // Add model result files to zip to be uploaded
                        for ( const auto& result : model_ctrl->get_output_filenames( std::to_string( i ), tconfig.exptid ) ) {
                            fs::path fpath = upload_dir;
                            fpath /= result;
                            if ( path_exists( fpath.string() ) ) {
                                std::cerr << "Adding to the zip: " << fpath << '\n';
                                zfl.push_back( fpath );
                            }
                        }
                    }

                    std::string upload_file = bconfig.project_dir + result_base_name + "_" + std::to_string( tstate.upload_file_number ) + ".zip";
                    std::cerr << "Compressing upload file: " << upload_file << '\n';

                    // Create the zipped upload file from the list of files added to zfl
                    if ( !zfl.empty() ) {
                        auto zret = zip_and_delete( upload_file, zfl );

                        // If running under a BOINC client
                        if ( !bconfig.standalone && zret == 0 ) {

                            // Upload the file. In BOINC the upload file is the logical name, not the physical name
                            std::string upload_file_name = "upload_file_" + std::to_string( tstate.upload_file_number ) + ".zip";
                            std::cerr << "Uploading the intermediate file: " << upload_file_name << '\n';

                            std::this_thread::sleep_until( chrono::system_clock::now() + chrono::seconds( 20 ) );
                            retval = boinc_upload_file( upload_file_name );
                            if ( retval ) {
                                std::cerr << "..boinc_upload_file failed for file: " << upload_file_name << std::endl;
                                return task_finish( retval );
                            }
                            retval = boinc_upload_status( upload_file_name );
                            if ( !retval ) {
                                std::cerr << "Finished the upload of the intermediate file: " << upload_file_name << '\n';
                            }
                        }
                        tstate.last_upload = tstate.current_step;
                    }
                    tstate.last_upload = tstate.current_step;

                    // *****  Normal end of critical section  *****
                    boinc_end_critical_section();
                    tstate.upload_file_number++;

                }    // end of upload new output file block.

                // Trickle every required fraction of the model run
                if ( ( step_value % trickle_freq ) == 0 ) {
                    std::cerr << "Sending progress trickle message to CPDN at step: " << step << '\n';
                    trickler.process_trickle( tstate.current_cpu_time, tstate.current_step );
                    tstate.last_trickle_step = tstate.current_step;
                }
            }    // end of if it's a new timestep block.

            tstate.last_step = step;
            delay_count = 0;

            // Update progress file with current values
            if ( !progress_file.write( tstate, err_msg ) ) {
                std::cerr << "..Failed to write progress file: " << err_msg << '\n';
                return task_finish( 1 );
            }
        }

        // Calculate current_cpu_time, only update if cpu_time returns a value
        if ( cpdn_cpu_time( tstate.pid ) > 0 ) {
            tstate.current_cpu_time = tstate.last_cpu_time + cpdn_cpu_time( tstate.pid );
        }

        // Calculate the fraction done
        tstate.fraction_done = model_frac_done( std::stof( step ), total_nsteps, nthreads_int );

        if ( !bconfig.standalone ) {
            // If the current model step is at a restart interval, update restart cpu time for boinc.
            double restart_cpu_time = 0;
            int step_value = 0;
            std::string step_str = step;
            if ( !parse_int( step_str, step_value, err_msg ) ) {
                std::cerr << "..Failed to parse current step: " << err_msg << '\n';
                return task_finish( 1 );
            }
            if ( !( step_value % restart_interval ) ) {
                restart_cpu_time = tstate.current_cpu_time;
            }

            // Provide the current cpu_time to the BOINC server (note: this is deprecated in BOINC)
            boinc_report_app_status( tstate.current_cpu_time, restart_cpu_time, tstate.fraction_done );

            // Provide the fraction done to the BOINC client, necessary for the percentage bar on the client
            boinc_fraction_done( tstate.fraction_done );

            tstate.process_status = check_boinc_status( tstate.pid, tstate.process_status );
        }

        tstate.process_status = check_child_status( tstate.pid, tstate.process_status, tstate.exit_code );
    }

    //----- End of main loop ---------------------------------------------------------------------------

    // Do NOT execute a return until the final upload is done after the boinc_end_critical_section() below.

    // GC. I probably don't need this; use the task_process_status variable & model_success instead in main loop?
    tstate.model_completed = 1;

    // Time delay to ensure model files are all flushed to disk
    sleep_seconds( 60 );

    tstate.model_success = model_ctrl->check_model_success( ifs_stat );

    if ( tstate.model_success ) {
        std::cerr << "..Model completed successfully" << std::endl;
    } else {
        std::cerr << "..Failed, model did not complete successfully" << std::endl;
        std::cerr << "..Model exit code: " << tstate.exit_code << std::endl;
    }

    // Print the model logs & progress file (if they exist)
    std::cerr << ".. Printing tail of model log files .." << std::endl;
    model_ctrl->print_logs( 40 );
    std::cerr << "... Printing controller progress file .. " << std::endl;
    progress_file.print( std::cerr );


    //-----------------------------Create the final results zip file-----------------------------------------

    // Although the final move of output files may have failed above, there might still be some previous
    // output files in the upload dir ready to be zipped and uploaded.

    boinc_begin_critical_section();

    zfl.clear();
    std::string node_file = bconfig.slot_path + "/NODE.001_01";
    if ( path_exists( node_file ) ) {
        zfl.push_back( node_file );
        std::cerr << "Adding to the zip: " << node_file << '\n';
    }
    std::string ifsstat_file = bconfig.slot_path + "/ifs.stat";
    if ( path_exists( ifsstat_file ) ) {
        zfl.push_back( ifsstat_file );
        std::cerr << "Adding to the zip: " << ifsstat_file << '\n';
    }

    // Move the final model result files ready for upload
    for ( const auto& result : model_ctrl->get_output_filenames( tstate.last_step, tconfig.exptid ) ) {
        retval = move_result_file( bconfig.slot_path, upload_dir, result );
        if ( retval ) {
            std::cerr << "..Copying " << result << " model result file to the temp folder in the projects directory failed" << "\n";
        }
    }

    // Read the remaining list of files from the temp upload directory and
    // add the matching files to the upload zip
    retval = add_upload_files( upload_dir, zfl, model_ctrl->get_output_filename_regex() );
    if ( retval ) {
        std::cerr << "Adding model output files to the upload zip failed!\n";
    }

    std::string upload_file = bconfig.project_dir + result_base_name + "_" + std::to_string( tstate.upload_file_number ) + ".zip";
    std::cerr << "Compressing final upload file: " << upload_file << '\n';

    if ( !zfl.empty() ) {
        auto zret = zip_and_delete( upload_file, zfl );

        if ( !bconfig.standalone && zret == 0 ) {

            std::string upload_file_name = "upload_file_" + std::to_string( tstate.upload_file_number ) + ".zip";
            std::cerr << "Uploading the final file: " << upload_file_name << '\n';

            std::this_thread::sleep_until( chrono::system_clock::now() + chrono::seconds( 20 ) );
            retval = boinc_upload_file( upload_file_name );
            if ( retval ) {
                std::cerr << "..boinc_upload_file failed for file: " << upload_file_name << std::endl;
            } else {
                retval = boinc_upload_status( upload_file_name );
                if ( !retval ) {
                    std::cerr << "Finished the upload of the final file" << '\n';
                }
            }

            // Produce final trickle it's the same timestep as the last main loop trickle
            if ( tstate.current_step > tstate.last_trickle_step ) {
                trickler.process_trickle( tstate.current_cpu_time, tstate.current_step );
            }
        }
    }

    //-------------------------------------------------------------------------------------------------------

    // Now that the task has finished, remove the temp folder
    fs::remove_all( upload_dir );

    boinc_end_critical_section();

    // Delay to ensure all files are flushed to disk before exiting
    std::cerr << "Waiting for all file operations to complete..." << std::endl;
    sleep_seconds( 90 );
    std::cerr << "Task finished." << std::endl;

    // if finished normally
    if ( tstate.process_status == 1 || tstate.process_status == 2 ) {
        return task_finish( 0 );
    } else {
        return task_finish( 1 );    // I could return the task return code here?
    }
}

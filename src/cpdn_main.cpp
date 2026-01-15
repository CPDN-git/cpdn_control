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
#include <regex.h>    // this for regex matching of output files.
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
#include "api/trickle_handler.h"

#include "models/test/test_control.h"

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
static std::unique_ptr<ModelControl> create_model_control( std::string_view vendor, std::string_view model_name,
                                                           std::string_view model_version,
                                                           std::string_view primary_ctrl_file )
{
    std::unique_ptr<ModelControl> model;    // create a null unique_ptr ready for a new model control instance.

    // Model mappings

    if ( model_name == "test_model" ) {
        model = std::make_unique<TestControl>( vendor, model_name, model_version, primary_ctrl_file );
    }
    if ( model_name == "oifs_43r3" ) {
        //model_ctrl = std::make_unique<OpenIFSControl>("ECMWF", "OpenIFS", "43r3", "fort.4");
    }

    return model;
}


/**
 * @brief Parse and validate the --nthreads argument from app_config.xml.
 * 
 * @param app_config_nthreads The string value of the nthreads argument.
 * @param nthreads Altered number of threads as a string.
 * @returns True if the nthreads argument was valid and changed, false otherwise.
 */
static bool get_app_config_nthreads( const std::string& app_config_nthreads, std::string& nthreads )
{

    if ( app_config_nthreads.empty() ) {
        std::cerr << "Warning. --nthreads argument present but has no value! Ignoring.\n";
    } else {
        try {
            int max_threads =
                8;    // GC. This is the best maximum as parallel efficiency markedly drops after this many threads, even at T319.
            int min_threads = 1;    // minimum number of threads.
            int ithreads = -1;

            ithreads = std::stoi( app_config_nthreads );
            if ( ithreads > max_threads ) {
                std::cerr << "Warning. --nthreads value is too high. Setting to max number of threads : " << max_threads
                          << '\n';
                nthreads = std::to_string( max_threads );
            } else if ( ithreads < min_threads ) {
                std::cerr
                    << "Warning. --nthreads is too low for this configuration. Minimum #threads is 2. Resetting.\n";
                nthreads = std::to_string( min_threads );
            }
            return true;
        } catch ( ... ) {
            std::cerr << "Warning. --nthreads argument must be a valid integer! Ignoring.\n";
        }
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
                  << "Usage: " << argv[0]
                  << " <start_date> <exptid> <unique_member_id> <batchid> <wuid> <fclen> [app_version]\n";
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

    boinc_finish(
        exit_code );    // boinc_finish exits, no further code executed after this call (unless a dummy library is used).
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
    std::cerr << "Workunit name: " << bconfig.wu_name << '\n'
              << "CPDN project directory: " << bconfig.project_dir << '\n';

    // ---------------- Task configuration -----------------

    // TODO. Read in the model config.xml.  The XML file contains all the information
    // about the model. It's required to initialize the correct model class later on.

    // Check for existence of model_config.xml in current directory (task) and fail if not found.
    if ( !path_exists( MODEL_CONFIG_FILE ) ) {
        std::cerr << "..The model config.xml file does not exist in the current directory: " << MODEL_CONFIG_FILE
                  << std::endl;
        //GC. Testing only; return task_finish(1);        // should terminate, the model won't run.
    }

    // Create model control instance.
    // In future, rather than pass app_name, we might pass the model name read from model_config.xml.
    // "CPDN" and "fort.4" are placeholders for vendor name and primary control file respectively.
    auto model_ctrl = create_model_control( "CPDN", bconfig.app_name, bconfig.app_version, "fort.4" );
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
    if ( std::string( argv[argc - 2] ) == "--nthreads" ) {
        std::string app_config_nthreads = argv[argc - 1];
        get_app_config_nthreads( app_config_nthreads, nthreads );
        bconfig.ncpus = std::stoi( nthreads );
        std::cerr << "Using --nthreads from app_config.xml: " << nthreads << '\n';
    }

    const std::string namelist = "fort.4";    // namelist file. will come from XML input later.
    double num_days = atof(
        tconfig.fclen.c_str() );    // number of simulation days; fclen should come from fort.4, not the command line.

    // --------------- Prepare the task environment -----------------

    boinc_begin_critical_section();

    // Create temp upload folder for moving the results to and uploading the results from.
    // BOINC measures the disk usage on the slots directory so we must move all results out of this folder
    std::string upload_dir = bconfig.project_dir + bconfig.app_name + "_" + tconfig.wuid;
    std::cerr << "Location of temp folder: " << upload_dir << '\n';
    if ( !path_exists( upload_dir ) ) {
        if ( mkdir( upload_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH ) != 0 ) {
            std::cerr << "..mkdir for temp folder for results failed" << std::endl;
            boinc_end_critical_section();
            return task_finish( 1 );    // should terminate, the model won't run.
        }
    }

    //  Unpack application into slot
    retval = move_and_unzip_app_file( bconfig.app_name, bconfig.app_version, bconfig.project_dir, bconfig.slot_path );
    if ( retval ) {
        std::cerr << "..move_and_unzip_app_file failed" << "\n";
        boinc_end_critical_section();
        return task_finish( retval );
    }

    //------------------------------------------Process the namelist-----------------------------------------
    // GC. Note, this is not the 'model fort.4' namelist file being referred to here. Needs renaming to avoid confusion.
    // This should really be part of a general piece of code to process the model ancil files. Needs refactoring later.

    fs::path namelist_zip_path = bconfig.slot_path;
    namelist_zip_path /= std::string( bconfig.app_name ) + "_" + tconfig.unique_member_id + "_" + tconfig.start_date +
                         "_" + std::to_string( (int)num_days ) + "_" + tconfig.batchid + "_" + tconfig.wuid + ".zip";
    std::string namelist_zip = namelist_zip_path.string();    // nb this is a const string.

    // Copy the namelist_zip to the slot directory and unzip
    if ( copy_and_unzip( namelist_zip, namelist_zip, bconfig.slot_path, "namelist_zip" ) ) {
        std::cerr << "..Copying and unzipping the namelist_zip failed: " << namelist_zip << std::endl;
        boinc_end_critical_section();
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
        boinc_end_critical_section();
        return task_finish( 1 );    // should terminate, the model won't run.
    }

    // Read the model's controlling namelist file

    if ( !( namelist_filestream.is_open() ) ) {
        namelist_filestream.open( namelist_file );
    }
    if ( !namelist_filestream.is_open() ) {
        std::cerr << "..Error opening namelist file: " << namelist_file << std::endl;
        boinc_end_critical_section();
        return task_finish( 1 );
    }

    std::string parsed_key;
    std::string parsed_value;

    // Parsing the namelist file at the moment is a mix of looking for CPDN injected
    // header variables and normal model namelist variables. It's a bit clumsy but works for now.

    // The header_keys are the CPDN injected task related parameters always at the
    // top of the namelist file. It's not a tidy solution as they are prefixed with
    // '!' making them normal comments. This causes issues parsing them because if we
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
    const std::unordered_set<std::string> header_keys = { "IC_ANCIL_FILE",    "IFSDATA_FILE",    "CLIMATE_DATA_FILE",
                                                          "HORIZ_RESOLUTION", "VERT_RESOLUTION", "GRID_TYPE",
                                                          "UPLOAD_INTERVAL" };

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
                     header_keys.find( parsed_key ) !=
                         header_keys.end() ) {    // ignore any keys not in the header list above
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
            try {
                upload_interval = std::stoi( tmpstr );
            } catch ( ... ) {
                std::cerr << ".. Warning, unable to parse upload interval from namelist, setting to zero, got string: "
                          << tmpstr << '\n';
                upload_interval = 0;
            }
        } else if ( parsed_key == "UTSTEP" ) {
            tmpstr = parsed_value;
            try {
                timestep = std::stoi( tmpstr );
            } catch ( ... ) {
                std::cerr
                    << ".. Warning, unable to parse timestep interval from namelist, setting to zero, got string: "
                    << tmpstr << '\n';
                timestep = 0;
            }
        } else if ( parsed_key ==
                    "NFRPOS" ) {    // frequency of model OUTPUT file creation (for upload); +ve model steps, -ve hours.
            tmpstr = parsed_value;
            try {
                ICM_file_interval = std::stoi( tmpstr );
            } catch ( ... ) {
                std::cerr << ".. Warning, unable to parse ICM model output interval from namelist, setting to zero, "
                             "got string: "
                          << tmpstr << std::endl;
                ICM_file_interval = 0;
            }
        } else if ( parsed_key ==
                    "NFRRES" ) {    // frequency of model RESTART file creation: +ve model steps, -ve hours.
            tmpstr = parsed_value;
            try {
                restart_interval = stoi( tmpstr );
            } catch ( ... ) {
                std::cerr << "..Warning, unable to parse restart interval from namelist, setting to zero, got string: "
                          << tmpstr << std::endl;
                restart_interval = 0;
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
        boinc_end_critical_section();
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
            boinc_end_critical_section();
            return task_finish( 1 );    // should terminate, the model won't run.
        }
    }

    // Copy the ifsdata_zip to the slot directory and unzip
    // GC TODO. convert to fs::path and get rid of handling '/'
    std::string ifsdata_check = ifsdata_folder + "/";
    if ( copy_and_unzip( ifsdata_zip, ifsdata_destination, ifsdata_check, "ifsdata_zip" ) ) {
        std::cerr << "..Copying and unzipping the ifsdata_zip failed: " << ifsdata_zip << std::endl;
        boinc_end_critical_section();
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
            boinc_end_critical_section();
            return task_finish( 1 );
        }
    }

    // Copy the climate_data_zip to the slot directory and unzip
    if ( copy_and_unzip( climate_data_zip, climate_data_destination, climate_data_path, "climate_data_zip" ) ) {
        std::cerr << "..Copying and unzipping the climate_data_zip failed: " << climate_data_zip << std::endl;
        boinc_end_critical_section();
        return task_finish( 1 );    // should terminate, the model won't run.
    }

    //-------------------------------------------------------------------------------------------------------

    // Define the name and location of the progress file and the rcf file
    std::string progress_file = bconfig.slot_path + "/progress_file";
    std::string rcf_file = bconfig.slot_path + "/rcf";

    TaskState task;    // Initialize task state with default values

    // Check whether the rcf file and the progress file (contains model progress) are not already present from an unscheduled shutdown
    std::cerr << "Checking for rcf file and progress file: " << progress_file << '\n';

    // Handle the cases of the various states of the rcf file and progress file
    if ( !path_exists( progress_file ) && !path_exists( rcf_file ) ) {
        // If both progress file and rcf file do not exist, then model has not run.
        // Do nothing as the task state variables are already initialized to zero values above.
        std::cerr << "-- Starting new model run --\n";
    } else if ( path_exists( progress_file ) && file_is_empty( progress_file ) ) {
        // If progress file exists and is empty, an error has occurred, then kill model run
        model_ctrl->print_logs( 50 );
        std::cerr << "..progress file exists, but is empty => problem with model, quitting run" << '\n';
        return task_finish( 1 );
    } else if ( path_exists( progress_file ) && !path_exists( rcf_file ) ) {
        read_progress_file( progress_file, task );
        // If last_iter less than the restart interval, then model is at beginning and rcf has yet to be produced then continue
        if ( std::stoi( task.last_iter ) >= restart_interval ) {
            // Otherwise if progress file exists and rcf file does not exist, an error has occurred, then kill model run
            model_ctrl->print_logs( 50 );
            std::cerr << "..progress file exists, but rcf file does not exist => problem with model, quitting run"
                      << '\n';
            return task_finish( 1 );
        }
    } else if ( !path_exists( progress_file ) && path_exists( rcf_file ) ) {
        // If rcf file exists and progress file does not exist, an error has occurred, then kill model run
        // TODO: we should be able to bootstrap the progress file from the rcf file here?
        model_ctrl->print_logs( 50 );
        std::cerr << "..rcf file exists, but progress file does not exist => problem with model, quitting run" << '\n';
        return task_finish( 1 );
    } else if ( ( path_exists( progress_file ) && !file_is_empty( progress_file ) ) && path_exists( rcf_file ) ) {
        // If progress file exists and is not empty and rcf file exists, then read rcf file and progress file
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

        read_progress_file( progress_file, task );

        // Check if the CSTEP variable from rcf is greater than the last_iter, if so then quit model run
        if ( stoi( cstep_value ) > stoi( task.last_iter ) ) {
            std::cerr << "..CSTEP variable from rcf is greater than last_iter from progress file, error has occurred, "
                         "quitting model run"
                      << '\n';
            return task_finish( 1 );
        }

        // Adjust last_iter to the step of the previous model restart dump step.
        // This is always a multiple of the restart frequency

        std::cerr << "-- Model is restarting --\n";
        std::cerr << "Adjusting last_iter, " << task.last_iter << ", to previous model restart step.\n";
        int restart_iter = stoi( task.last_iter );
        restart_iter = restart_iter - ( ( restart_iter % restart_interval ) -
                                        1 );    // -1 because the model will continue from restart_iter.
        task.last_iter = std::to_string( restart_iter );
    }

    // Update progress file with current values
    update_progress_file( progress_file, task );

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

    if ( !set_exec_perms( exe_cmd ) ) {
        std::cerr << "..Cannot start model. Setting execute permission for model executable failed: " << exe_cmd
                  << std::endl;
        return task_finish( 1 );
    }

    // Start the model process
    std::cerr << "Launching model executable: " << exe_cmd << std::endl;
    task.pid = launch_process( bconfig.project_dir, bconfig.slot_path, exe_cmd, nthreads, tconfig.exptid );
    if ( task.pid > 0 )
        task.process_status =
            0;    //GC TODO. Need to handle when task.pid =-1, i.e. launch failed (see code in launch_process_oifs)

    boinc_end_critical_section();


    // process_status = 0 running
    // process_status = 1 stopped normally
    // process_status = 2 stopped with quit request from BOINC
    // process_status = 3 stopped with child process being killed
    // process_status = 4 stopped with child process being stopped
    // process_status = 5 child process not found by waitpid()


    //----------------------------------------Main loop------------------------------------------------------

    // Periodically check the process status and the BOINC client status
    std::string stat_lastline;
    std::string second_part;
    std::string ifs_stat = bconfig.slot_path + "/ifs.stat";    // GC. TODO: should be std::filesystem path.

    std::vector<fs::path> zfl;

    int count = 0;
    std::string iter = "0";

    while ( task.process_status == 0 && task.model_completed == 0 ) {
        sleep_seconds( 1 );    // Time delay to reduce overhead

        count++;

        // Check whether an upload point has been reached
        // GC. 09/25. reduced to 7 secs as testing shows 10secs can miss a timestep.
        // Going too low can cause the %age done on boincmgr to flip backwards.
        if ( count == 7 ) {

            iter = task.last_iter;
            if ( path_exists( ifs_stat ) ) {

                // Read completed step from last line of ifs.stat file.
                // Note the first line from the model has a step count of '....  CNT3      -999 ....'
                // When the iteration number changes in the ifs.stat file, OpenIFS has completed writing
                // to the output files for that iteration, those files can now be moved and uploaded.
                //std::cerr << "Reading completed iteration step from last line of ifs.stat" << std::endl;

                if ( fread_last_line( ifs_stat, stat_lastline ) ) {       // only returns true if lastline has changed
                    if ( oifs_parse_stat( stat_lastline, iter, 4 ) ) {    // iter updates
                        if ( !oifs_valid_step( iter, total_nsteps ) ) {
                            iter = task.last_iter;    // revert to last valid step
                        }
                    }
                }
            }

            if ( std::stoi( iter ) != std::stoi( task.last_iter ) ) {
                // Construct file name of the ICM result file
                second_part = oifs_get_filename_part( task.last_iter, tconfig.exptid );

                // Move the ICMGG, ICMSH & ICMUA result files to the task folder in the project directory
                // GC. Why do this every timestep? This should be done at same frequency as NFRPOS.
                std::vector<std::string> icm = { "ICMGG", "ICMSH", "ICMUA" };

                for ( const auto& part : icm ) {
                    std::string result = part + second_part;
                    retval = move_result_file( bconfig.slot_path, upload_dir, result );
                    if ( retval ) {
                        std::cerr << "..Copying " << part
                                  << " result file to the temp folder in the projects directory failed" << "\n";
                        return task_finish( retval );
                    }
                }

                // Convert iteration number to seconds
                task.current_iter = ( std::stoi( task.last_iter ) ) * timestep;

                // Upload a new upload file if the end of an upload_interval has been reached
                if ( ( ( task.current_iter - task.last_upload ) >= ( upload_interval * timestep ) ) &&
                     ( task.current_iter < total_length_of_simulation ) ) {
                    // Create an intermediate results zip file
                    zfl.clear();

                    std::cerr << "End of upload interval reached, starting a new upload process" << std::endl;

                    // *****  Critical section -- all returns must now call boinc_end_critical_section()  *****
                    boinc_begin_critical_section();

                    // Cycle through all the steps from the last upload to the current upload
                    for ( auto i = ( task.last_upload / timestep ); i < ( task.current_iter / timestep );
                          i++ ) {    //  task.current_iter/timestep is just task.last_iter!

                        // Construct file name of the ICM result file
                        second_part = oifs_get_filename_part( std::to_string( i ), tconfig.exptid );

                        // Add ICM result files to zip to be uploaded
                        for ( const auto& part : icm ) {
                            fs::path fpath = upload_dir;
                            fpath /= part + second_part;
                            if ( path_exists( fpath.string() ) ) {
                                std::cerr << "Adding to the zip: " << fpath << '\n';
                                zfl.push_back( fpath );
                            }
                        }
                    }

                    std::string upload_file = bconfig.project_dir + result_base_name + "_" +
                                              std::to_string( task.upload_file_number ) + ".zip";
                    std::cerr << "Compressing upload file: " << upload_file << '\n';

                    // Create the zipped upload file from the list of files added to zfl
                    if ( !zfl.empty() ) {
                        auto zret = zip_and_delete( upload_file, zfl );

                        // If running under a BOINC client
                        if ( !bconfig.standalone && zret == 0 ) {

                            // Upload the file. In BOINC the upload file is the logical name, not the physical name
                            std::string upload_file_name =
                                "upload_file_" + std::to_string( task.upload_file_number ) + ".zip";
                            std::cerr << "Uploading the intermediate file: " << upload_file_name << '\n';

                            std::this_thread::sleep_until( chrono::system_clock::now() + chrono::seconds( 20 ) );
                            retval = boinc_upload_file( upload_file_name );
                            if ( retval ) {
                                std::cerr << "..boinc_upload_file failed for file: " << upload_file_name << std::endl;
                                boinc_end_critical_section();
                                return task_finish( retval );
                            }
                            retval = boinc_upload_status( upload_file_name );
                            if ( !retval ) {
                                std::cerr << "Finished the upload of the intermediate file: " << upload_file_name
                                          << '\n';
                            }
                        }
                        task.last_upload = task.current_iter;
                    }
                    task.last_upload = task.current_iter;

                    // *****  Normal end of critical section  *****
                    boinc_end_critical_section();
                    task.upload_file_number++;

                }    // end of upload new output file block.

                // Trickle every required fraction of the model run
                if ( ( std::stoi( iter ) % trickle_freq ) == 0 ) {
                    std::cerr << "Sending progress trickle message to CPDN at step: " << iter << '\n';
                    trickler.process_trickle( task.current_cpu_time, task.current_iter );
                    task.last_trickle_iter = task.current_iter;
                }
            }    // end of if it's a new timestep block.
            task.last_iter = iter;
            count = 0;

            // Update progress file with current values
            update_progress_file( progress_file, task );
        }

        // Calculate current_cpu_time, only update if cpu_time returns a value
        if ( cpdn_cpu_time( task.pid ) > 0 ) {
            task.current_cpu_time = task.last_cpu_time + cpdn_cpu_time( task.pid );
        }

        // Calculate the fraction done
        task.fraction_done = model_frac_done( std::stof( iter ), total_nsteps, std::stoi( nthreads ) );

        if ( !bconfig.standalone ) {
            // If the current iteration is at a restart iteration
            double restart_cpu_time = 0;
            if ( !( std::stoi( iter ) % restart_interval ) ) {
                restart_cpu_time = task.current_cpu_time;
            }

            // Provide the current cpu_time to the BOINC server (note: this is deprecated in BOINC)
            boinc_report_app_status( task.current_cpu_time, restart_cpu_time, task.fraction_done );

            // Provide the fraction done to the BOINC client, necessary for the percentage bar on the client
            boinc_fraction_done( task.fraction_done );

            task.process_status = check_boinc_status( task.pid, task.process_status );
        }

        task.process_status = check_child_status( task.pid, task.process_status, task.exit_code );
    }

    //----- End of main loop ---------------------------------------------------------------------------

    // Do NOT execute a return until the final upload is done, after the boinc_end_critical_section() below.

    // GC. I probably don't need this; use the task_process_status variable & model_success instead in main loop?
    task.model_completed = 1;

    // Time delay to ensure model files are all flushed to disk
    sleep_seconds( 60 );

    // To check whether model completed successfully, look for 'CNT0' in 3rd column of ifs.stat
    // This will always be the last line of a successful model forecast.
    // TODO: This needs to be a function call.

    task.model_success = false;    // default to false unless confirmed below.
    if ( path_exists( ifs_stat ) ) {
        std::string ifs_word = "";
        fread_last_line( ifs_stat, stat_lastline );
        oifs_parse_stat( stat_lastline, ifs_word, 3 );
        std::cerr << "Last line of ifs.stat, ifs_word: " << stat_lastline << ", " << ifs_word << '\n';
        if ( ifs_word != "CNT0" ) {
            std::cerr << "CNT0 not found; string returned was: " << "'" << ifs_word << "'" << '\n';
        } else {
            task.model_success = true;    // <<< only point at which model success is set to true <<<
        }
    }

    if ( task.model_success ) {
        std::cerr << "..Model completed successfully" << std::endl;
    } else {
        std::cerr << "..Failed, model did not complete successfully" << std::endl;
        std::cerr << "..Model exit code: " << task.exit_code << std::endl;
    }

    // Print the model logs & progress file (if they exist)
    std::cerr << ".. Printing tail of model log files .." << std::endl;
    model_ctrl->print_logs( 40 );
    std::cerr << "... Printing controller progress file .. " << std::endl;
    print_last_lines( progress_file, 10 );

    // Move the final ICMGG, ICMSH and ICMUA model output files to the task folder in the project directory
    second_part = oifs_get_filename_part( task.last_iter, tconfig.exptid );

    std::vector<std::string> icm = { "ICMGG", "ICMSH", "ICMUA" };
    for ( const auto& part : icm ) {
        std::string result = part + second_part;
        retval = move_result_file( bconfig.slot_path, upload_dir, result );
        if ( retval ) {
            std::cerr << "..Copying " << part << " result file to the temp folder in the projects directory failed"
                      << "\n";
        }
    }

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

    // Read the remaining list of files from the slots directory and add the matching files to the list of files for the zip
    // GC. TODO. Update to C++ 17.
    if ( auto* dirp = opendir( upload_dir.c_str() ) ) {
        regex_t regex;
        regcomp( &regex, "\\+", 0 );

        struct dirent const* dir;
        while ( ( dir = readdir( dirp ) ) != nullptr ) {

            if ( !regexec( &regex, dir->d_name, (size_t)0, nullptr, 0 ) ) {
                zfl.push_back( upload_dir + "/" + dir->d_name );
                std::cerr << "Adding to the zip: " << ( upload_dir + "/" + dir->d_name ) << '\n';
            }
        }
        regfree( &regex );
        closedir( dirp );
    }

    std::string upload_file =
        bconfig.project_dir + result_base_name + "_" + std::to_string( task.upload_file_number ) + ".zip";
    std::cerr << "Compressing final upload file: " << upload_file << '\n';

    if ( !zfl.empty() ) {
        auto zret = zip_and_delete( upload_file, zfl );

        if ( !bconfig.standalone && zret == 0 ) {

            std::string upload_file_name = "upload_file_" + std::to_string( task.upload_file_number ) + ".zip";
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
            if ( task.current_iter > task.last_trickle_iter ) {
                trickler.process_trickle( task.current_cpu_time, task.current_iter );
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
    if ( task.process_status == 1 || task.process_status == 2 ) {
        return task_finish( 0 );
    } else {
        return task_finish( 1 );    // I could return the task return code here?
    }
}

//
// BOINC task controller for CPDN.
//
// This version written by Glenn Carver, CPDN, 2025.
// Original version by Andy Bowery (Oxford eResearch Centre, Oxford University) December 2023.
//

#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>  // this and...
#include <regex.h>   // this for regex matching of output files.

#include "boinc/boinc_api.h"

#include "cpdn_zip.h"
#include "cpdn_control.h"
#include "lib/utils.h"
#include "api/trickle_handler.h"

// these includes will disappear when the code moves to Model derived classes
#include "models/openifs/oifs_utils.h" // for get_second_part, oifs_*() functions.


namespace chrono = std::chrono;
namespace     fs = std::filesystem;


// Define the code version if not defined at compile time with -D option.
#ifndef CODE_VERSION
 #define CODE_VERSION "1.0.0"
#endif


// Constants
constexpr std::string_view  MODEL_CONFIG_FILE = "model_config.xml";



/**
 * @brief Main program for the CPDN task controller.
 */
int main(int argc, char** argv)
{
    BoincConfig config;
    int retval=0;

    // Initialise BOINC to get the project directory, workunit name and app version
    // Note this redirects stderr output to stderr.txt in slot dir.
    retval = init_boinc(config);
    if (retval) {
       std::cerr << "..BOINC initialisation failed" << "\n";
       return retval;
    }
    if ( config.standalone ) std::cerr << "Running in standalone mode" << '\n';

    // Set the task related paths
    // GC. TODO. make these fs::path variables.
    std::string slot_path = fs::current_path();
    if (slot_path.empty()) {
      std::cerr << "..Error. Can't determine slot path: current_path() returned empty" << std::endl;
      return 1;
    }
    std::cerr << "Working slot directory is: "<< slot_path << '\n';
    
    std::cerr << "Project directory is: " << config.project_dir << '\n';

    // Say who we are.
    banner("CPDN task controller", CODE_VERSION);    //  will come from XML input later.

    // TODO. Read in the model config.xml.  The XML file contains all the information
    // about the model. It's required to initialize the correct model class later on.

    // Check for existence of config.xml in current directory (task) and fail if not found.
    if( !path_exists(MODEL_CONFIG_FILE) ) {
        std::cerr << "..The model config.xml file does not exist in the current directory: " << MODEL_CONFIG_FILE << std::endl;
        //GC. Testing only; return 1;        // should terminate, the model won't run.
    }

    // Argument processing; at least 9 args always.
    // TODO: THIS NEEDS TO BE HANDLED BY THE SPECIFIC MODEL CLASS
    if (argc < 9) {
        std::cerr << "CPDN Controller error: Not enough command line arguments provided.\n"
                  << "Usage: " << argv[0] << " <start_date> <exptid> <unique_member_id> <batchid> <wuid> <fclen> <app_name> <nthreads> [app_version]\n";
        return 1;
    }
    std::cerr << "(argv0) " << argv[0] << '\n'
              << "(argv1) start_date: " << argv[1] << '\n'
              << "(argv2) exptid: " << argv[2] << '\n'
              << "(argv3) unique_member_id: " << argv[3] << '\n'
              << "(argv4) batchid: " << argv[4] << '\n'
              << "(argv5) wuid: " << argv[5] << '\n'
              << "(argv6) fclen: " << argv[6] << '\n'
              << "(argv7) app_name: " << argv[7] << '\n'
              << "(argv8) nthreads: " << argv[8] << std::endl;

    // Read the exptid, umid, batchid, wuid, fclen, app_name, number of threads from the command line
    // argv[9] if present, is assigned below
    std::string start_date = argv[1]; // simulation start date
    std::string exptid = argv[2];     // OpenIFS experiment id
    std::string unique_member_id = argv[3];  // umid
    std::string batchid = argv[4];    // batch id
    std::string wuid = argv[5];       // workunit id
    std::string fclen = argv[6];      // number of simulation days
    std::string app_name = argv[7];   // CPDN app name
    std::string nthreads = argv[8];   // number of OPENMP threads.
    std::string app_config_nthreads;  // blank initially.

    // Check for optional '--nthreads <value>' at end of arg list, optionally set by app_config.xml on user's machine.
    if ( std::string(argv[argc - 2]) == "--nthreads" ) {
      app_config_nthreads = argv[argc-1];

      if ( app_config_nthreads.empty() ) {
         std::cerr << "Warning. --nthreads argument present but has no value! Ignoring.\n";
      }
      else {
         try {
            int max_nthreads = 8;      // GC. This is the best maximum as T319 parallel efficiency markedly drops after this many threads.
            int min_nthreads = 2;
            int i_nthreads = -1;

            i_nthreads = std::stoi(app_config_nthreads);
            if ( i_nthreads > max_nthreads ) {
               std::cerr << "Warning. --nthreads value is too high. Setting to max number of threads : " << max_nthreads << '\n';
               i_nthreads = max_nthreads;
            }
            else if ( i_nthreads < min_nthreads ) {
               std::cerr << "Warning. --nthreads is too low for this configuration. Minimum #threads is 2. Resetting.\n";
               i_nthreads = min_nthreads;
            }
            else {
               //*****************************************************
               // Uncomment this line to enable the --nthreads argument
               //nthreads = i_nthreads
            }
         }
         catch (...) {
            std::cerr << "Warning. --nthreads argument must be a valid integer! Ignoring.\n";
         }
      }
    }

    std::cerr << "\nCPDN task controller version: " << CODE_VERSION << '\n' // CODE_VERSION is a macro set at compile time
              << "wu_name: " << config.wu_name << '\n'
              << "project_dir: " << config.project_dir << '\n'
              << "version: " << config.version << '\n';

    const std::string namelist="fort.4";    // namelist file. will come from XML input later.

    double num_days = atof(fclen.c_str());   // number of simulation days; fclen should come from fort.4, not the command line.

    if (!config.standalone) {

      // Get the app version and re-parse to add a dot
      // GC. This assumes version is X.Y, X.YY or XX.YY format, will get it wrong if not.
      auto vlen = config.version.length();
      if ( vlen == 2 ) {
         config.version.insert(1, ".");
      }
      else if (vlen > 2 ) {
         config.version.insert(vlen-2, ".");
      }

      std::cerr << "app name: " << app_name << '\n'
                << "version: " << config.version << '\n';
    }
    // Running in standalone
    else {

      // Set the project path. Assume usual boinc dir structure.
      config.project_dir = slot_path + std::string("/../../projects/");
      std::cerr << "Project directory is: " << config.project_dir << '\n';

      // In standalone get the app version from the command line
      config.version = argv[9];
      std::cerr << "app name: " << app_name << '\n'
                << "(argv9) app_version: " << argv[9] << '\n'; 
    }

    boinc_begin_critical_section();

    // Create temp upload folder for moving the results to and uploading the results from.
    // BOINC measures the disk usage on the slots directory so we must move all results out of this folder
    std::string upload_dir = config.project_dir + app_name + "_" + wuid;
    std::cerr << "Location of temp folder: " << upload_dir << '\n';
    if ( !path_exists(upload_dir) ) {
      if (mkdir(upload_dir.c_str(),S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) != 0) {
         std::cerr << "..mkdir for temp folder for results failed" << std::endl;
      }
    }

    //  Unpack application into slot
    retval = move_and_unzip_app_file(app_name, config.version, config.project_dir, slot_path);
    if (retval) {
      std::cerr << "..move_and_unzip_app_file failed" << "\n";
      return retval;
    }

    //------------------------------------------Process the namelist-----------------------------------------
    // GC. Note, this is not the 'model fort.4' namelist file being referred to here. Needs renaming to avoid confusion.
    // This should really be part of a general piece of code to process the model ancil files. Needs refactoring later.
    
   fs::path namelist_zip_path = slot_path;
   namelist_zip_path /= std::string(app_name) + "_" +
                       unique_member_id + "_" +
                       start_date + "_" +
                       std::to_string((int)num_days) + "_" +
                       batchid + "_" +
                       wuid + ".zip";
   std::string namelist_zip = namelist_zip_path.string();      // nb this is a const string.

	// Copy the namelist_zip to the slot directory and unzip
    if ( copy_and_unzip(namelist_zip, namelist_zip, slot_path, "namelist_zip") ) {
       std::cerr << "..Copying and unzipping the namelist_zip failed: " << namelist_zip << std::endl;
       return 1;        // should terminate, the model won't run.
	}


    // Parse the fort.4 namelist for the filenames and variables
    std::string ifsdata_file;
    std::string ic_ancil_file;
    std::string climate_data_file;
    std::string namelist_file = slot_path + "/" + namelist;
    std::string namelist_line;
    std::string horiz_resolution;
    std::string vert_resolution;
    std::string grid_type;
    std::string tmpstr;

    std::ifstream namelist_filestream;

    char equals = '=';

    int upload_interval = 0;
    int trickle_freq = 0;
    int timestep = 0;
    int ICM_file_interval = 0;
    int restart_interval = 0;

    // Check for the existence of the namelist
    if( !path_exists(namelist_file) ) {
       std::cerr << "..The namelist file does not exist: " << namelist_file << std::endl;
       return 1;        // should terminate, the model won't run.
    }

    // Open the namelist file
    if(!(namelist_filestream.is_open())) {
       namelist_filestream.open(namelist_file);
    }

    // Read the namelist file
    // GC. Recoded. Removed unneccesary use of istringstream, moved code to new function, and fixed incorrect length used in substr().
    //     Also fixed bug reading string '!NFRPOS', should have been 'NFRPOS', meaning it was never assigned correct value.
    while(std::getline(namelist_filestream, namelist_line))
    {
       tmpstr.clear();

       if ( extract_key_value( namelist_line,"IFSDATA_FILE", equals, ifsdata_file ) ) {
       }
       else if ( extract_key_value( namelist_line, "IC_ANCIL_FILE", equals, ic_ancil_file ) ) {
       }
       else if ( extract_key_value( namelist_line, "CLIMATE_DATA_FILE", equals, climate_data_file ) ) {
       }
       else if ( extract_key_value( namelist_line, "HORIZ_RESOLUTION", equals, horiz_resolution ) ) {
       }
       else if ( extract_key_value( namelist_line, "VERT_RESOLUTION", equals, vert_resolution ) ) {
       }
       else if ( extract_key_value( namelist_line, "GRID_TYPE", equals, grid_type ) ) {
       }
       else if ( extract_key_value( namelist_line, "UPLOAD_INTERVAL", equals, tmpstr ) ) {
          try {
            upload_interval=std::stoi(tmpstr);
          }
          catch (...) {
            std::cerr << ".. Warning, unable to parse upload interval from namelist, setting to zero, got string: " << tmpstr << '\n';
            upload_interval = 0;
          }
       }
       else if ( extract_key_value( namelist_line, "UTSTEP", equals, tmpstr) ) {
          try {
            timestep = std::stoi(tmpstr);
          }
          catch (...) {
            std::cerr << ".. Warning, unable to parse timestep interval from namelist, setting to zero, got string: " << tmpstr << '\n';
            timestep = 0;
          }
       }
       else if ( extract_key_value( namelist_line, "NFRPOS", equals, tmpstr) ) {   // frequency of model OUTPUT file creation (for upload); +ve model steps, -ve hours.
          try {
            ICM_file_interval = std::stoi(tmpstr);
          }
          catch (...) {
            std::cerr << ".. Warning, unable to parse ICM model output interval from namelist, setting to zero, got string: " << tmpstr << std::endl;
            ICM_file_interval = 0;
          }
       }
       else if ( extract_key_value( namelist_line, "NFRRES", equals, tmpstr) ) {     // frequency of model RESTART file creation: +ve model steps, -ve hours.
          try {
            restart_interval = stoi(tmpstr);
          }
          catch (...) {
            std::cerr << "..Warning, unable to parse restart interval from namelist, setting to zero, got string: " << tmpstr << std::endl;
            restart_interval = 0;
          }
       }
    }
    namelist_filestream.close();

    // Check for any empty variables in case parsing failed.
    // These might cause the task to fail later, or they might be deliberate for testing.
    if ( ifsdata_file.empty() ) {
      std::cerr << ".. Warning. Unable to parse ifs_data_file from namelist.\n";
    }
    if ( ic_ancil_file.empty() ) {
      std::cerr << ".. Warning. Unable to parse ic_ancil_file from namelist.\n";
    }
    if ( climate_data_file.empty() ) {
      std::cerr << ".. Warning. Unable to parse climate_data_file from namelist.\n";
    }
    if ( horiz_resolution.empty() ) {
      std::cerr << ".. Warning. Unable to parse horiz_resolution from namelist.\n";
    }
    if ( vert_resolution.empty() ) {
      std::cerr << ".. Warning. Unable to parse vert_resolution from namelist.\n";
    }
    if ( grid_type.empty() ) {
      std::cerr << ".. Warning. Unable to parse grid_type from namelist.\n";
    }

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
       restart_interval = abs(restart_interval)*3600 / timestep;
       std::cerr << " NFRRES: restart dump frequency (in steps) " << restart_interval << '\n';
    }

    // this should match CUSTOP in fort.4. If it doesn't we have a problem.
    double total_nsteps = (num_days * 86400.0) / (double) timestep;     //GC. why is this a double? it's always an int.

    //GC. Oct/25. Trickles are now fixed at every 10% of the model run with a final trickle at the end of the run.
    //    Value read from fort.4 namelist is ignored and should be removed.
    trickle_freq = TrickleHandler::get_trickle_frequency(timestep, (int)total_nsteps);

    std::cerr << "Trickle frequency is every 10% of model run : " << trickle_freq << " model steps, "
                << ((float)trickle_freq*(float)timestep)/86400.0 << " days.\n";

    //-------------------------------------------------------------------------------------------------------
    //    Unpack the task's input files into the slot directory

    // Process the ic_ancil_file:
    std::string ic_ancil_zip = slot_path + "/" + ic_ancil_file + ".zip";

	// Copy the ic_ancil_zip to the slot directory and unzip
    if ( copy_and_unzip(ic_ancil_zip, ic_ancil_zip, slot_path, "ic_ancil_zip") ) {
       std::cerr << "..Copying and unzipping the ic_ancil_zip failed: " << ic_ancil_zip << std::endl;
       return 1;        // should terminate, the model won't run.
	}

    // Process the ifsdata_file:
    // Make the ifsdata directory and set the required paths
    std::string ifsdata_folder = slot_path + "/ifsdata";
    std::string ifsdata_zip    = slot_path + "/" + ifsdata_file + ".zip";
    std::string ifsdata_destination = ifsdata_folder + "/" + ifsdata_file + ".zip";
    
    // Check if ifsdata folder does not already exists or is empty
    if ( !path_exists(ifsdata_folder) ) {
       if (mkdir(ifsdata_folder.c_str(),S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) != 0) {
          std::cerr << "..mkdir for ifsdata folder failed" << std::endl;
          return 1;        // should terminate, the model won't run.
       }
    }
    
    // Copy the ifsdata_zip to the slot directory and unzip
    // GC TODO. convert to fs::path and get rid of handling '/'
    std::string ifsdata_check = ifsdata_folder + "/";
    if ( copy_and_unzip(ifsdata_zip, ifsdata_destination, ifsdata_check, "ifsdata_zip") ) {
       std::cerr << "..Copying and unzipping the ifsdata_zip failed: " << ifsdata_zip << std::endl;
       return 1;        // should terminate, the model won't run.
    }

    // Process the climate_data_file:
    // Make the climate data directory and set the required paths
    std::string climate_data_path = slot_path + "/" + horiz_resolution + grid_type;
    std::string climate_data_zip = slot_path + "/" + climate_data_file + ".zip";
    std::string climate_data_destination = climate_data_path + "/" + climate_data_file + ".zip";
    
    // Check if climate_data folder does not already exists or is empty
    if ( !path_exists(climate_data_path) ) {
       if (mkdir(climate_data_path.c_str(),S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) != 0) {
          std::cerr << "..mkdir for the climate data folder failed" << std::endl;
          return 1;
       }
    }               
       
    // Copy the climate_data_zip to the slot directory and unzip
    if ( copy_and_unzip(climate_data_zip, climate_data_destination, climate_data_path, "climate_data_zip") ) {
       std::cerr << "..Copying and unzipping the climate_data_zip failed: " << climate_data_zip << std::endl;
       return 1;        // should terminate, the model won't run.
    }

    //-------------------------------------------------------------------------------------------------------

    // Define the name and location of the progress file and the rcf file
    std::string progress_file = slot_path + "/progress_file";
    std::string rcf_file = slot_path + "/rcf";

    TaskState task;  // Initialize task state with default values

    // Check whether the rcf file and the progress file (contains model progress) are not already present from an unscheduled shutdown
    std::cerr << "Checking for rcf file and progress file: " << progress_file << '\n';

    // Handle the cases of the various states of the rcf file and progress file
    if ( !path_exists(progress_file) && !path_exists(rcf_file) ) {
       // If both progress file and rcf file do not exist, then model has not run.
       // Do nothing as the task state variables are already initialized to zero values above.
       std::cerr << "-- Starting new model run --\n";
    }
    else if ( path_exists(progress_file) && file_is_empty(progress_file) ) {
       // If progress file exists and is empty, an error has occurred, then kill model run
       print_last_lines("NODE.001_01", 70);
       print_last_lines("ifs.stat",8);
       std::cerr << "..progress file exists, but is empty => problem with model, quitting run" << '\n';
       return 1;
    }
    else if ( path_exists(progress_file) && !path_exists(rcf_file) ) {
       read_progress_file(progress_file, task);
       // If last_iter less than the restart interval, then model is at beginning and rcf has yet to be produced then continue
       if (std::stoi(task.last_iter) >= restart_interval) {
          // Otherwise if progress file exists and rcf file does not exist, an error has occurred, then kill model run
          print_last_lines("NODE.001_01", 70);
          print_last_lines("ifs.stat",8);
          std::cerr << "..progress file exists, but rcf file does not exist => problem with model, quitting run" << '\n';
          return 1;
       }
    }
    else if ( !path_exists(progress_file) && path_exists(rcf_file) ) {
       // If rcf file exists and progress file does not exist, an error has occurred, then kill model run
       // TODO: we should be able to bootstrap the progress file from the rcf file here?
       print_last_lines("NODE.001_01", 70);
       print_last_lines("ifs.stat",8);
       std::cerr << "..rcf file exists, but progress file does not exist => problem with model, quitting run" << '\n';
       return 1;
    }
    else if ( (path_exists(progress_file) && !file_is_empty(progress_file)) && path_exists(rcf_file) ) {
       // If progress file exists and is not empty and rcf file exists, then read rcf file and progress file
       std::ifstream rcf_file_stream;
       std::string ctime_value;
       std::string cstep_value;

       // Read the rcf file
       if( path_exists( rcf_file ) ) {
         if( !(rcf_file_stream.is_open()) ) {
            rcf_file_stream.open( rcf_file );
         }
         if( rcf_file_stream.is_open() ) {
            if (oifs_read_rcf_file(rcf_file_stream, ctime_value, cstep_value)) {
               std::cerr << "Read the rcf file" << '\n';
            }
            else {
               // Reading the rcf file failed, then kill model run
               print_last_lines("NODE.001_01", 70);
               print_last_lines("ifs.stat",8);
               std::cerr << "..Reading the rcf file failed" << '\n';
	            return 1;
            }
         }
       }
       rcf_file_stream.close();

       read_progress_file(progress_file, task);

       // Check if the CSTEP variable from rcf is greater than the last_iter, if so then quit model run
       if ( stoi(cstep_value) > stoi(task.last_iter) ) {
          std::cerr << "..CSTEP variable from rcf is greater than last_iter from progress file, error has occurred, quitting model run" << '\n';
          return 1;
       }

       // Adjust last_iter to the step of the previous model restart dump step.
       // This is always a multiple of the restart frequency

       std::cerr << "-- Model is restarting --\n";
       std::cerr << "Adjusting last_iter, " << task.last_iter << ", to previous model restart step.\n";
       int restart_iter = stoi(task.last_iter);
       restart_iter = restart_iter - ((restart_iter % restart_interval) - 1);   // -1 because the model will continue from restart_iter.
       task.last_iter = std::to_string(restart_iter); 
    }

    // Update progress file with current values
    update_progress_file(progress_file, task);

    // seconds between upload files: upload_interval
    // seconds between ICM files: ICM_file_interval * timestep
    // upload interval in steps = upload_interval / timestep
    //cerr "upload_interval: "<< upload_interval << ", timestep: " << timestep << '\n';

    // Check if upload_interval x timestep equal to zero
    if (upload_interval * timestep == 0) {
       std::cerr << "..upload_interval x timestep equals zero" << std::endl;
       return 1;
    }

    int total_length_of_simulation = (int) (num_days * 86400);
    std::cerr << "Total_length_of_simulation: " << total_length_of_simulation << '\n';

    // Get result_base_name to construct upload file names using 
    // the first upload as an example and then stripping off '_0.zip'

    std::string result_base_name;

    if (!config.standalone) {
       std::string resolved_name;
       retval = boinc_resolve_filename_s("upload_file_0.zip", resolved_name);
       if (retval) {
          std::cerr << "..boinc_resolve_filename failed" << std::endl;
          return 1;
       }

       result_base_name = fs::path(resolved_name).stem(); // returns filename without path nor '.zip'
       if ( result_base_name.length() > 2 ){
          result_base_name.erase( result_base_name.length() - 2 );     // remove the '_0'
       }

       std::cerr << "result_base_name: " << result_base_name << '\n';
       if (result_base_name.compare("upload_file") == 0) {
          std::cerr << "..Failed to get result name" << std::endl;
          return 1;
       }
    }

    // Create the trickle handler (only trickle if not in standalone mode)
    TrickleHandler trickler(config.wu_name, result_base_name, slot_path);

    // Determine which OpenIFS executable to run.
    // GC. This should be an input parameter on the command line.

    fs::path single_proc_exe = slot_path;
             single_proc_exe /= "oifs_43r3_model.exe";
    fs::path multi_proc_exe  = slot_path;
             multi_proc_exe /= "oifs_43r3_omp_model.exe";
    fs::path test_proc_exe   = slot_path;
             test_proc_exe /= "oifs_43r3_test.exe";
    std::string exe_cmd{};

    if ( path_exists(single_proc_exe.string()) ) {
       exe_cmd = single_proc_exe.string();
    }
    else if ( path_exists(multi_proc_exe.string()) ) {
       exe_cmd = multi_proc_exe.string();
    }
    else if ( path_exists(test_proc_exe.string()) ) {
       exe_cmd = test_proc_exe.string();
    }
    if (exe_cmd.empty()) {
       std::cerr << "..No OpenIFS executable found, ending task." << std::endl;
       return 1;
    }

    // Bug workaround. The current cpdn_unzip function does not preserve executable permissions on Linux.
    // Manually set the permissions on the OpenIFS executable before running.

    if ( !set_exec_perms(exe_cmd) ) {
       std::cerr << "..Cannot start model. Setting execute permission for OpenIFS executable failed: " << exe_cmd << std::endl;
       return 1;
    }

    // Start the OpenIFS job
    std::cerr << "Launching OpenIFS executable: " << exe_cmd << std::endl;
    long model_process = launch_process(config.project_dir, slot_path, exe_cmd, nthreads, exptid, app_name);
    if (model_process > 0) task.process_status = 0;     //GC TODO. Need to handle when model_process =-1, i.e. launch failed (see code in launch_process_oifs)


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
    std::string ifs_stat = slot_path + "/ifs.stat";     // GC. TODO: should be std::filesystem path.

    std::vector<fs::path> zfl;

    int count = 0;
    std::string iter = "0";

    while (task.process_status == 0 && task.model_completed == 0)
    {
       sleep_seconds(1);         // Time delay to reduce overhead

       count++;

       // Check whether an upload point has been reached
       // GC. 09/25. reduced to 7 secs as testing shows 10secs can miss a timestep.
       // Going too low can cause the %age done on boincmgr to flip backwards.
       if(count==7) {

          iter = task.last_iter;
          if ( path_exists(ifs_stat) ) {

             // Read completed step from last line of ifs.stat file.
             // Note the first line from the model has a step count of '....  CNT3      -999 ....'
             // When the iteration number changes in the ifs.stat file, OpenIFS has completed writing
             // to the output files for that iteration, those files can now be moved and uploaded.
             //std::cerr << "Reading completed iteration step from last line of ifs.stat" << std::endl;

             if ( fread_last_line(ifs_stat, stat_lastline) ) {       // only returns true if lastline has changed
                 if ( oifs_parse_stat(stat_lastline, iter, 4) ) {    // iter updates
                    if ( !oifs_valid_step(iter,total_nsteps) ) {
                       iter = task.last_iter;                             // revert to last valid step
                    }
                 }
             }
          }

          if (std::stoi(iter) != std::stoi(task.last_iter)) {
             // Construct file name of the ICM result file
             second_part = get_second_part(task.last_iter, exptid);

             // Move the ICMGG, ICMSH & ICMUA result files to the task folder in the project directory
             // GC. Why do this every timestep? This should be done at same frequency as NFRPOS.
             std::vector<std::string> icm = {"ICMGG", "ICMSH", "ICMUA"};
             for (const auto& part : icm) {
               std::string result = part + second_part;
               retval = move_result_file(slot_path, upload_dir, result);
               if (retval) {
                  std::cerr << "..Copying " << part << " result file to the temp folder in the projects directory failed" << "\n";
                  return retval;
               }
             }

             // Convert iteration number to seconds
             task.current_iter = (std::stoi(task.last_iter)) * timestep;

             //std::cerr << "Current iteration of model: " << task.last_iter << '\n';
             //std::cerr << "timestep: " << timestep << '\n';
             //std::cerr << "current_iter: " << task.current_iter << '\n';
             //std::cerr << "last_upload: " << task.last_upload << '\n';

             // Upload a new upload file if the end of an upload_interval has been reached
             if((( task.current_iter - task.last_upload ) >= (upload_interval * timestep)) && (task.current_iter < total_length_of_simulation)) {
                // Create an intermediate results zip file
                zfl.clear();

                std::cerr << "End of upload interval reached, starting a new upload process" << std::endl;

                // *****  Critical section -- all returns must now call boinc_end_critical_section()  *****
                boinc_begin_critical_section();

                // Cycle through all the steps from the last upload to the current upload
                for (auto i = (task.last_upload / timestep); i < (task.current_iter / timestep); i++) {   //  task.current_iter/timestep is just task.last_iter!
                   //std::cerr << "last_upload/timestep: " << (task.last_upload/timestep) << '\n';
                   //std::cerr << "current_iter/timestep: " << (task.current_iter/timestep) << '\n';
                   //std::cerr << "i: " << (std::to_string(i)) << '\n';

                   // Construct file name of the ICM result file
                   second_part = get_second_part(std::to_string(i), exptid);

                   // Add ICM result files to zip to be uploaded
                   std::vector<std::string> icm = {"ICMGG", "ICMSH", "ICMUA"};
                   for (const auto& part : icm) {
                      fs::path  fpath = upload_dir;
                                fpath /= part + second_part;
                      if (path_exists(fpath.string())) {
                         std::cerr << "Adding to the zip: " << fpath << '\n';
                         zfl.push_back(fpath);
                      }
                   }
                }

                // If running under a BOINC client
                if (!config.standalone) {

                   if (zfl.size() > 0)
                   {
                      // Create the zipped upload file from the list of files added to zfl
                      std::string upload_file = config.project_dir + result_base_name + "_" + std::to_string(task.upload_file_number) + ".zip";

                      std::cerr << "Compressing upload file: " << upload_file << '\n';

                      // Time the compression for diagnostics
                      auto start = chrono::high_resolution_clock::now();
                      auto outcome = cpdn_zip(upload_file, zfl);
                      auto stop = chrono::high_resolution_clock::now();
                      auto duration = chrono::duration_cast<chrono::milliseconds>(stop - start);
                      std::cerr << "Time taken to compress upload file: " << duration.count() << " ms\n";

                      retval = outcome ? 0 : 1;

                      if (retval) {
                         std::cerr << ".. compressing upload file failed" << std::endl;
                         boinc_end_critical_section();
                         return retval;
                      }
                      else {
                         // Files have been successfully zipped, they can now be deleted
                         for (auto j = 0; j < (int) zfl.size(); ++j) {
                            // Delete the zipped file
                            try {
                                fs::remove(zfl[j]);
                            } catch (const fs::filesystem_error& e) {
                                std::cerr << "Error deleting file: " << zfl[j] << ", error: " << e.what() << '\n';
                            }
                         }
                      }

                      // Upload the file. In BOINC the upload file is the logical name, not the physical name
                      std::string upload_file_name = "upload_file_" + std::to_string(task.upload_file_number) + ".zip";
                      std::cerr << "Uploading the intermediate file: " << upload_file_name << '\n';
                      std::this_thread::sleep_until(chrono::system_clock::now() + chrono::seconds(20));
                      retval = boinc_upload_file(upload_file_name);
                      if (retval) {
                         std::cerr << "..boinc_upload_file failed for file: " << upload_file_name << std::endl;
                         boinc_end_critical_section();
                         return retval;
                      }
                      retval = boinc_upload_status(upload_file_name);
                      if (!retval) {
                         std::cerr << "Finished the upload of the intermediate file: " << upload_file_name << '\n';
                      }
                   }
                   task.last_upload = task.current_iter; 
                }

                // Else running in standalone
                else {
                   std::string upload_file_name = app_name + "_" + unique_member_id + "_" + start_date + "_" + \
                                                  std::to_string((int)num_days) + "_" + batchid + "_" + wuid + "_" + \
                                                  std::to_string(task.upload_file_number) + ".zip";
                   std::cerr << "The current upload_file_name is: " << upload_file_name << '\n';

                   // Create the zipped upload file from the list of files added to zfl
                   std::string upload_file = config.project_dir + upload_file_name;

                   if (zfl.size() > 0){
                      if (!cpdn_zip(upload_file, zfl)) {
                         retval = 1;
                      }

                      if (retval) {
                         std::cerr << "..Creating the zipped upload file failed" << std::endl;
                         boinc_end_critical_section();
                         return retval;
                      }
                      else {
                         // Files have been successfully zipped, they can now be deleted
                         for (auto j = 0; j < (int) zfl.size(); ++j) {
                            // Delete the zipped file
                            try {
                                fs::remove(zfl[j]);
                            } catch (const fs::filesystem_error& e) {
                                std::cerr << "Error deleting file: " << zfl[j] << ", error: " << e.what() << '\n';
                            }
                         }
                      }
                   }
                   task.last_upload = task.current_iter;

                }

                // *****  Normal end of critical section  *****
                boinc_end_critical_section();
                task.upload_file_number++;
             }                            // end of upload new output file block.

             // Trickle every required fraction of the model run
             if ( (std::stoi(iter) % trickle_freq) == 0 ) {
               std::cerr << "Sending progress trickle message to CPDN at step: " << iter << '\n';
               trickler.process_trickle(task.current_cpu_time, task.current_iter);
               task.last_trickle_iter = task.current_iter;
             }
          }                               // end of if it's a new timestep block.
          task.last_iter = iter;
          count = 0;

          // Update progress file with current values
          update_progress_file(progress_file, task);
       }

       // Calculate current_cpu_time, only update if cpu_time returns a value
       if (cpu_time(model_process)) {
          task.current_cpu_time = task.last_cpu_time + cpu_time(model_process);
       }

      // Calculate the fraction done
      task.fraction_done = model_frac_done( std::stof(iter), total_nsteps, std::stoi(nthreads) );

      if (!config.standalone) {
         // If the current iteration is at a restart iteration
         double restart_cpu_time = 0;
         if ( !(std::stoi(iter)%restart_interval)) {
            restart_cpu_time = task.current_cpu_time;
         }

         // Provide the current cpu_time to the BOINC server (note: this is deprecated in BOINC)
         boinc_report_app_status(task.current_cpu_time, restart_cpu_time, task.fraction_done);

         // Provide the fraction done to the BOINC client, necessary for the percentage bar on the client
         boinc_fraction_done(task.fraction_done);
    
         task.process_status = check_boinc_status(model_process, task.process_status);
      }
   
      task.process_status = check_child_status(model_process, task.process_status);
    }

    //----- End of main loop ---------------------------------------------------------------------------	


    // Time delay to ensure model files are all flushed to disk
    sleep_seconds(60);

    // Print content of key model files to help with diagnosing problems
    print_last_lines("NODE.001_01", 70);    //  main model output log	

    // To check whether model completed successfully, look for 'CNT0' in 3rd column of ifs.stat
    // This will always be the last line of a successful model forecast.
    if (path_exists(ifs_stat))
    {
       std::string ifs_word="";
       fread_last_line(ifs_stat, stat_lastline);
       oifs_parse_stat(stat_lastline, ifs_word, 3);
       std::cerr << "Last line of ifs.stat, ifs_word: " << stat_lastline << ", " << ifs_word << '\n';
       if (ifs_word!="CNT0") {
         std::cerr << "CNT0 not found; string returned was: " << "'" << ifs_word << "'" << '\n';
         // print extra files to help diagnose fail
         print_last_lines("ifs.stat",8);
         print_last_lines("rcf",11);              // openifs restart control
         print_last_lines("waminfo",17);          // wave model restart control
         print_last_lines(progress_file,10);      // model progress file
         std::cerr << "..Failed, model did not complete successfully" << std::endl;
         return 1;
       }
    }
    // ifs.stat has not been produced, then model did not start
    else {
       std::cerr << "..Failed, model did not start" << std::endl;
       return 1;	    
    }

    // Update model_completed
    task.model_completed = 1;

    // Handle the last ICM files
    second_part = get_second_part(task.last_iter, exptid);

    // Move the ICMGG, ICMSH and ICMUA model output files to the task folder in the project directory
    std::vector<std::string> icm = {"ICMGG", "ICMSH", "ICMUA"};
    for (const auto& part : icm) {
       std::string result = part + second_part;
       retval = move_result_file(slot_path, upload_dir, result);
       if (retval) {
          std::cerr << "..Copying " << part << " result file to the temp folder in the projects directory failed" << "\n";
          return retval;
       }
    }

    boinc_begin_critical_section();

    //-----------------------------Create the final results zip file-----------------------------------------

    zfl.clear();
    std::string node_file = slot_path + "/NODE.001_01";
    zfl.push_back(node_file);
    std::string ifsstat_file = slot_path + "/ifs.stat";
    zfl.push_back(ifsstat_file);
    std::cerr << "Adding to the zip: " << node_file << '\n';
    std::cerr << "Adding to the zip: " << ifsstat_file << '\n';

    // Read the remaining list of files from the slots directory and add the matching files to the list of files for the zip
    // GC. TODO. Update to C++ 17.
    DIR *dirp = opendir(upload_dir.c_str());
    if (dirp)
    {
      regex_t regex;
      regcomp(&regex,"\\+",0);

      struct dirent *dir;
      while ((dir = readdir(dirp)) != NULL) {
         //std::cerr << "In temp folder: "<< dir->d_name << '\n';

         if (!regexec(&regex,dir->d_name,(size_t) 0,NULL,0)) {
            zfl.push_back(upload_dir + "/" + dir->d_name);
            std::cerr << "Adding to the zip: " << (upload_dir+"/" + dir->d_name) << '\n';
         }
      }
      regfree(&regex);
      closedir(dirp);
    }

    // If running under a BOINC client
    if (!config.standalone) {
       if (zfl.size() > 0){

          // Create the zipped upload file from the list of files added to zfl
          std::string upload_file = config.project_dir + result_base_name + "_" + std::to_string(task.upload_file_number) + ".zip";

          std::cerr << "Compressing final upload file: " << upload_file << '\n';

          // Time the compression for diagnostics
          auto start = chrono::high_resolution_clock::now();
          auto outcome = cpdn_zip(upload_file, zfl);
          auto stop = chrono::high_resolution_clock::now();
          auto duration = chrono::duration_cast<chrono::milliseconds>(stop - start);
          std::cerr << "Time taken to compress final upload file: " << duration.count() << " ms\n";
          
          retval = outcome ? 0 : 1;

          if (retval) {
             std::cerr << "..compressing final upload file failed" << std::endl;
             boinc_end_critical_section();
             return retval;
          }
          else {
             // Files have been successfully zipped, they can now be deleted
             for (auto j = 0; j < (int) zfl.size(); ++j) {
                // Delete the zipped file
                try {
                    fs::remove(zfl[j]);
                } catch (fs::filesystem_error& e) {
                    std::cerr << "Error deleting file: " << zfl[j] << ", error: " << e.what() << '\n';
                }
             }
          }

          // Upload the file. In BOINC the upload file is the logical name, not the physical name
          std::string upload_file_name = "upload_file_" + std::to_string(task.upload_file_number) + ".zip";
          std::cerr << "Uploading the final file: " << upload_file_name << '\n';
          std::this_thread::sleep_until(chrono::system_clock::now() + chrono::seconds(20));
          retval = boinc_upload_file(upload_file_name);
          if (retval) {
             std::cerr << "..boinc_upload_file failed for file: " << upload_file_name << std::endl;
             boinc_end_critical_section();
             return retval;
          }
          retval = boinc_upload_status(upload_file_name);
          if (!retval) {
             std::cerr << "Finished the upload of the final file" << '\n';
          }

	       // Produce final trickle it's the same timestep as the last main loop trickle
          if ( task.current_iter > task.last_trickle_iter ) {
            trickler.process_trickle(task.current_cpu_time, task.current_iter);
          }
       }
       boinc_end_critical_section();
    }

    // Else running in standalone
    else {
       std::string upload_file_name = app_name + "_" + unique_member_id + "_" + start_date + "_" + \
                                      std::to_string((int)num_days) + "_" + batchid + "_" + wuid + "_" + \
                                      std::to_string(task.upload_file_number) + ".zip";
       std::cerr << "The final upload_file_name is: " << upload_file_name << '\n';

       // Create the zipped upload file from the list of files added to zfl
       std::string upload_file = config.project_dir + upload_file_name;

       if (zfl.size() > 0) {
          if (!cpdn_zip(upload_file, zfl)) {
             retval = 1;
          }
          if (retval) {
             std::cerr << "..Creating the compressed upload file failed" << std::endl;
             boinc_end_critical_section();
             return retval;
          }
          else {
             // Files have been successfully zipped, they can now be deleted
             for (auto j = 0; j < (int) zfl.size(); ++j) {
                // Delete the zipped file
                try {
                  fs::remove(zfl[j]);
                } catch (const fs::filesystem_error& e) {
                  std::cerr << "Error deleting file: " << zfl[j] << ", error: " << e.what() << '\n';
                }
             }
         }
       }
    }

    //-------------------------------------------------------------------------------------------------------

    // Now that the task has finished, remove the temp folder
    fs::remove_all(upload_dir);

    boinc_end_critical_section();

    // Delay to ensure all files are flushed to disk before exiting
    sleep_seconds(120);
    std::cerr << "Task finished." << std::endl;

    // if finished normally
    if (task.process_status == 1){
      boinc_finish(0);     // boinc_finish() exits, no further code executed after this call.
      return 0;
    }
    else if (task.process_status == 2){
      boinc_finish(0);
      return 0;
    }
    else {
      boinc_finish(1);
      return 1;
    }	
}

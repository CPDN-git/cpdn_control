//
// Controller functions for CPDN BOINC model applications.
//
//    Glenn Carver, CPDN, 2025.
//
// Original code: Andy Bowery (Oxford eResearch Centre, Oxford University) May 2023
// Rewritten and refactored into class structure and modular form: Glenn Carver (CPDN), 2025->
//

#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <exception>
#include <vector>
#include <cerrno>
#include <cstring>

#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>   // for mkdir
#include <sys/resource.h>

#include "boinc/boinc_api.h"
#include "boinc/diagnostics.h"
#include "boinc/util.h"

#include "cpdn_control.h"
#include "cpdn_zip.h"
#include "lib/utils.h"

#include "models/openifs/oifs_utils.h" // for oifs_*() functions. Will be replaced by Model derived class later.

namespace chrono = std::chrono;
namespace     fs = std::filesystem;


/**
 * @brief Initialise BOINC and set the options
 */
// TODO: all the workunit parameters should be part of a struct/class.
int initialise_boinc(std::string& wu_name, std::string& project_dir, std::string& version, int& standalone) {

    //boinc_init_diagnostics(BOINC_DIAG_DEFAULTS);
    boinc_init();
    boinc_parse_init_data_file();

    // Get BOINC user preferences
    APP_INIT_DATA dataBOINC;
    boinc_get_init_data(dataBOINC);
    
    wu_name = dataBOINC.wu_name;
    project_dir = dataBOINC.project_dir;
    version = std::to_string(dataBOINC.app_version);

    // Set BOINC optional values
    BOINC_OPTIONS options;
    boinc_options_defaults(options);
    options.main_program = true;            // tell boinc client this is the main program
    // Nov/2025. This option appears to sometimes cause a memory corruption (!prev); possible race condition
    //          on 'static std::vector<UPLOAD_FILE_STATUS> upload_file_status'; in boinc_api.cpp upon destruction.  
    //          Disable for now. Monitor code did not use it.
    //          If this is set true, the client will kill all our child processes on exit (supposedly).
    //          It also appears to execute suspend/resume on child processes, which we do here.
    //          If I disable this, then we should make sure all descendants are killed on exit.
    //options.multi_process = true;           // if your app uses multiple processes, do this before creating any threads or processes, or storing the PID
    options.check_heartbeat = true;         // controller monitors 'heartbeat' messages from the client.
    options.handle_process_control = true;  // controller will handle all suspend/quit/resume messages from the boinc client.
    options.direct_process_action = false;  // controller will respond to quit messages and heartbeat failures by exiting, 
                                            // and will respond to suspend and resume messages by suspending and resuming
    options.send_status_msgs = false;       // If set, the program will report its CPU time and fraction done to the client. 
                                            // Set in worker programs.

    // Check whether BOINC is running in standalone mode
    standalone = boinc_is_standalone();
    
    return boinc_init_options(&options);
}



// Next function allows the use of an override file to set environment variables for testing
// on live tasks on remote machines.  The file is a simple text file with one variable per line in the format:
// VAR=VALUE  or export VAR='VALUE'  (single or double quotes can be used, or no quotes)
// e.g. export OMP_NUM_THREADS=6

/**
 * @brief Checks for the override file and sets environment variables if found.
 * * 
 * @param project_path The base directory.
 * @param filename The override environment filename.
 * @return true if environment variables were successfully processed, false otherwise.
 */
bool process_env_overrides(const fs::path& override_envs)
{
    if (!fs::exists(override_envs)) {
        // Fail silently to avoid highlighting existence of file
        //std::cerr << "Override file not found: " << override_envs.string() << std::endl;
        return false;
    }

    // debugging only. don't advertise existence of file
    //std::cerr << "Processing environment overrides from: " << override_envs.string() << std::endl;
    
    std::ifstream file(override_envs);
    if (!file.is_open()) {
        // Fail silently
        //std::cerr << "Error: Could not open override file for reading." << std::endl;
        return false;
    }

    std::string line;
    bool success = true;
    while (std::getline(file, line))
    {
        std::string var_name;
        std::string var_value;

        if (parse_key_value(line, var_name, var_value))
        {
            try {
                set_env_var(var_name, var_value);
                std::cerr << "Overriding env var: " << var_name << " = " << var_value << '\n';
            } 
            catch (const std::exception& e) {
                std::cerr << "Error setting variable: " << e.what() << std::endl;
                success = false;
            }
        }
    }

    return success;
}



/**
 * @brief Copies the application zip file to the slot directory and unzips it.
 * 
 * @param app_name     The name of the application.
 * @param version      The version string of the application.
 * @param project_path The path to the project directory.
 * @param slot_path    The path to the slot directory.
 * @return int         Returns 0 on success, non-zero on failure.
 */
int move_and_unzip_app_file(std::string app_name, std::string version, std::string project_path, std::string slot_path) {
   // GC. TODO. This code could be combined with copy_and_unzip() to avoid code duplication.

    int retval = 0;

    // macOS
    #if defined (__APPLE__)
       std::string app_file = app_name + "_app_" + version + "_x86_64-apple-darwin.zip";
    // ARM
    #elif defined (_ARM) 
       std::string app_file = app_name + "_app_" + version + "_aarch64-poky-linux.zip";
    // Linux
    #else
       std::string app_file = app_name + "_app_" + version + "_x86_64-pc-linux-gnu.zip";
    #endif

    // Copy the app file to the working directory
    fs::path app_source = project_path;
    app_source /= app_file;
    fs::path app_destination = slot_path;
    app_destination /= app_file;
    std::cerr << "Copying: " << app_source << " to: " << app_destination << "\n";

    // GC. Replace boinc copy with modern C++17 filesystem copy.  Overwrite to match boinc_copy behaviour.
    try {
       fs::copy_file(app_source, app_destination,  fs::copy_options::overwrite_existing);
    } 
    catch (const  fs::filesystem_error& e) {
      std::cerr << "..move_and_unzip_app: Error copying file: " << app_source << " to: " << app_destination << ",\nError: " << e.what() << std::endl;
      return 1;
    }

    // Unzip the app zipfile
    std::cerr << "Extracting the app zipfile: " << app_destination << "\n";

    if (!cpdn_unzip(app_destination, slot_path)){
       retval = 1;
       std::cerr << "..Extracting the app zipfile failed" << "\n";
       return retval;
    }
    else {
       try {
            fs::remove(app_destination);
       } catch (const  fs::filesystem_error& e) {
           std::cerr << "..move_and_unzip_app_file(). Error removing file: " << app_destination << ",\nError: " << e.what() << std::endl;
       }
    }
    return retval;
}



/**
 * @brief Checks the status of a child process.
 * @param handleProcess The process handle of the child process.
 * @param process_status The current status of the process.
 * @return The updated process status.
 */
int check_child_status(long handleProcess, int process_status) {
    int stat = 0;
    int pid = 0;

    // Check whether child processed has exited
    // waitpid will return process id of zombie (finished) process; zero if still running
    if ( (pid=waitpid(handleProcess,&stat,WNOHANG)) > 0 ) {
       process_status = 1;
       // Child exited normally but model might still have failed
       if (WIFEXITED(stat)) {
          process_status = 1;
          std::cerr << "..The child process terminated with status: " << WEXITSTATUS(stat) << '\n';
       }
       // Child process has exited due to signal that was not caught
       // n.b. OpenIFS has its own signal handler.
       else if (WIFSIGNALED(stat)) {
          process_status = 3;
          std::cerr << "..The child process has been killed with signal: " << WTERMSIG(stat) << '\n';
       }
       // Child is stopped
       else if (WIFSTOPPED(stat)) {
          process_status = 4;
          std::cerr << "..The child process has stopped with signal: " << WSTOPSIG(stat) << '\n';
       }
    }
    else if ( pid == -1) {
      // should not get here, it means the child could not be found
      process_status = 5;
      std::cerr << "..Unable to retrieve status of child process " << '\n';
      perror("waitpid() error");
    }
    return process_status;
}



/**
 * @brief Checks the BOINC client status and handles suspend, quit, and abort requests.
 * 
 * @param handleProcess The process handle of the child process.
 * @param process_status The current status of the process.
 * @return The updated process status.
 */
int check_boinc_status(long handleProcess, int process_status) {
    BOINC_STATUS status;
    boinc_get_status(&status);

    // If a quit, abort or no heartbeat has been received from the BOINC client, end child process
    if (status.quit_request) {
       std::cerr << "Quit request received from BOINC client, ending the child process" << '\n';
       kill(handleProcess, SIGKILL);
       process_status = 2;
       return process_status;
    }
    else if (status.abort_request) {
       std::cerr << "Abort request received from BOINC client, ending the child process" << '\n';
       kill(handleProcess, SIGKILL);
       process_status = 1;
       return process_status;
    }
    else if (status.no_heartbeat) {
       std::cerr << "No heartbeat received from BOINC client, ending the child process" << '\n';
       kill(handleProcess, SIGKILL);
       process_status = 1;
       return process_status;
    }
    // Else if BOINC client is suspended, suspend child process and periodically check BOINC client status
    else {
       if (status.suspended) {
          std::cerr << "Suspend request received from the BOINC client, suspending the child process" << '\n';
          kill(handleProcess, SIGSTOP);

          while (status.suspended) {
             boinc_get_status(&status);
             if (status.quit_request) {
                std::cerr << "Quit request received from the BOINC client, ending the child process" << '\n';
                kill(handleProcess, SIGKILL);
                process_status = 2;
                return process_status;
             }
             else if (status.abort_request) {
                std::cerr << "Abort request received from the BOINC client, ending the child process" << '\n';
                kill(handleProcess, SIGKILL);
                process_status = 1;
                return process_status;
             }
             else if (status.no_heartbeat) {
                std::cerr << "No heartbeat received from the BOINC client, ending the child process" << '\n';
                kill(handleProcess, SIGKILL);
                process_status = 1;
                return process_status;
             }
             sleep_seconds(1);
          }
          // Resume child process
          std::cerr << "Resuming the child process" << "\n";
          kill(handleProcess, SIGCONT);
          process_status = 0;
       }
       return process_status;
    }
}



/**
 * @brief Launches a child process to run the model executable.
 * 
 * @param project_path The path to the project directory.
 * @param slot_path The path to the slot directory.
 * @param strCmd The command to execute (model executable).
 * @param nthreads The number of threads to use.
 * @param exptid The experiment ID.
 * @param app_name The application name.
 * @return long The process handle of the launched child process, or -1 on failure.
 */
long launch_process(const std::string& project_path, const std::string& slot_path, 
                    const std::string& strCmd, const std::string& nthreads,
                    const std::string& exptid, const std::string& app_name)
{
    long handleProcess;

    switch((handleProcess=fork())) {
       case -1: {
          std::cerr << "..Unable to start a new child process" << "\n";
          return -1;      // Don't exit() here, return as this is the parent process.
          break;
       }
       case 0: { // The child process

           // Set the environment variables for the executable.
           if ( !oifs_setenvs(slot_path, nthreads) ) {
             std::cerr << "..Setting the OpenIFS environmental variables failed" << std::endl;
             exit(1);   // Can't continue child process so exit to end child process.
          }

          // --------------------------------------
          // Custom environment variable overrides, if the override file exists.
          // NOTE! This should only be used for testing and never advertised to users.
          {
            fs::path override_env_vars = project_path + "/oifs_override_env_vars";
            process_env_overrides(override_env_vars);
          }
          //---------------------------------------

          // Set the core dump size to 0
          struct rlimit core_limits;
          core_limits.rlim_cur = core_limits.rlim_max = 0;
          if (setrlimit(RLIMIT_CORE, &core_limits) != 0) {
             std::cerr << "..Setting the core dump size to 0 failed" << std::endl;
             exit(1);
          }

          // Set the stack limit to be unlimited
          struct rlimit stack_limits;
          // In macOS we cannot set the stack size limit to infinity
          #ifndef __APPLE__ // Linux
             stack_limits.rlim_cur = stack_limits.rlim_max = RLIM_INFINITY;
             if (setrlimit(RLIMIT_STACK, &stack_limits) != 0) {
                std::cerr << "..Setting the stack limit to unlimited failed" << std::endl;
             exit(1);
          }
          #endif

          // Execute model process.
          // OpenIFS 40r1 requires the -e exptid argument, later versions do not.
          // GC. TODO. This should be an input arg, not decided here.

          if( (app_name == "openifs") || (app_name == "oifs_40r1")) { // OpenIFS 40r1
            std::cerr << "Executing the command: " << strCmd << " -e " << exptid << "\n";
            execl(strCmd.c_str(),strCmd.c_str(),"-e",exptid.c_str(),NULL);
          }
          else {  // OpenIFS 43r3 and above
            std::cerr << "Executing the command: " << strCmd << "\n";
            execl(strCmd.c_str(),strCmd.c_str(),NULL);         // always returns -1 on failure
          }

          // If execl returns there was an error
          int syserr = errno;    // grab the error before any other system call.
          const char* syserr_msg = strerror(syserr);

          std::cerr << "..Launch process failed: execl - errno = " << syserr << ", " << syserr_msg
                     << "\n slot_path=" << slot_path << ",strCmd=" << strCmd << ",exptid=" << exptid << std::endl;

          exit(syserr);  // exit child process with system code for better remote diagnosis.
          break;
       }
       default: 
          std::cerr << "The child process has been launched with process id: " << handleProcess << "\n";
    }
    return handleProcess;
}



// Open a file and return the "jf_*" string contained between the arrow tags else empty string
// Explanation for Glenn's benefit :).  First run of task the filename contains a 'reference'
// to the real zip file stored in the projects directory.  The reference is in the form of a
// single line e.g. ">../../projects/climateprediction.net/jf_ic_ancil_1234<". 
// This function extracts the strings between the delimiters, so the real zip
// file can be copied to overwrite the file containing the 'jf_' file reference (yes, it's odd and not a good idea).
// On the subsequent runs, what should happen is the client restores the original
// 'reference' (jf_ic_ancil_1234) file. However, some clients do not do this which 
// means the real zip file is there instead. In this case get_tag returns an empty string.

// GC. Modified to avoid reading the entire zip file if it's not a jf_ file reference.
//     Otherwise we end up with a very big string in memory!
std::string get_tag(const std::string &filename) {

    constexpr auto MAX_READ_BYTES = 256;
    std::string buffer(MAX_READ_BYTES, '\0');

    std::ifstream file(filename, std::ios::in);

    if (!file.is_open()) {
         std::cerr << "..get_tag. Failed to open file: " << filename << std::endl;
        return std::string();
    }

    // Read up to MAX_READ_BYTES directly into the string's underlying buffer
    file.read(buffer.data(), MAX_READ_BYTES-1);          // Leave space for final null terminator set above
    
    // Get the actual number of bytes read
    std::streamsize chars_read = file.gcount();

    if (chars_read == 0) {
       return std::string(); // File is empty
    }

    // Check for the "magic number" for zipfiles in case zipfile has already been copied.
    if (chars_read > 2 && buffer[0] == 'P' && buffer[1] == 'K' ) {
       return std::string();
    }

    // Resize string to actual number of chars read
    buffer.resize(chars_read);

    // Look for the delimiters to find the file reference
    // We could still be unlucky here and have a binary file which might just
    // have a > and < in our buffer with garbage in. Negligible risk.
    const char START_TAG = '>';
    const char END_TAG = '<';

    auto start_pos = buffer.find(START_TAG);

    if (start_pos == std::string::npos) {
        return std::string();
    }

    auto tag_start = start_pos + 1;
    auto tag_end   = buffer.find(END_TAG, tag_start);

    if (tag_end == std::string::npos) {
        return std::string();
    }

    // Extract the file reference
    // The length is (position of '<') - (position after '>')
    auto length = tag_end - tag_start;
    
    return buffer.substr(tag_start, length);
}



/**
 * @brief Reads task progress from progress file into TaskState struct
 * @param progress_file Path to the progress file to read
 * @param task Reference to TaskState struct to populate
 */
void read_progress_file(std::string_view progress_file, TaskState& task) {

    // Parse the progress_file
    std::string progress_line = "";
    std::string delimiter = "=";
    std::ifstream progress_filestream;
    
    // Open the progress_file file
    if(!(progress_filestream.is_open())) {
       progress_filestream.open(std::string(progress_file));
    } 
    
    // Read the namelist file
    while(std::getline(progress_filestream, progress_line)) { //get 1 row as a string

       if (progress_line.find("last_cpu_time") != std::string::npos) {
          task.last_cpu_time = std::stoi(progress_line.substr(progress_line.find(delimiter)+1, progress_line.length()-1));
       }
       else if (progress_line.find("upload_file_number") != std::string::npos) {
          task.upload_file_number = std::stoi(progress_line.substr(progress_line.find(delimiter)+1, progress_line.length()-1));
       }
       else if (progress_line.find("last_iter") != std::string::npos) {
          task.last_iter = progress_line.substr(progress_line.find(delimiter)+1, progress_line.length()-1);
       }
       else if (progress_line.find("last_upload") != std::string::npos) {
          task.last_upload = std::stoi(progress_line.substr(progress_line.find(delimiter)+1, progress_line.length()-1));
       }
       else if (progress_line.find("model_completed") != std::string::npos) {
          task.model_completed = std::stoi(progress_line.substr(progress_line.find(delimiter)+1, progress_line.length()-1));
       }
    }
    progress_filestream.close();
}



/**
 * @brief Store task progress from TaskState struct to progress file
 * @param progress_file Path to the progress file to write
 * @param task Reference to TaskState struct containing progress information
 */
void update_progress_file(std::string_view progress_file, const TaskState& task)
{
    std::ofstream progress_file_out{std::string(progress_file)};

    // Write out the new progress file. Note this truncates progress_file to zero bytes if it already exists (as in a model restart)
    // GC Oct/2025. Make progress file a fortran namelist, so the models can easily read it to check the control process is still running.
    //              Also include controller pid so running model has additional way to check if controller is still alive.
    progress_file_out << "! CPDN controller progress file & fortran namelist\n"
                      << "&CPDN\n"
                      << "control_pid=" << std::to_string(getpid()) << '\n'
                      << "last_cpu_time=" << std::to_string(task.last_cpu_time) << '\n'
                      << "upload_file_number=" << std::to_string(task.upload_file_number) << '\n'
                      << "last_iter=" << task.last_iter << '\n'
                      << "last_upload=" << std::to_string(task.last_upload) << '\n'
                      << "model_completed="<< std::to_string(task.model_completed) << '\n'
                      << "/" << std::endl;
    progress_file_out.close();
}



/**
 * @brief Returns fraction completed of model run
 *        This is currently based on OpenIFS but should be general enough for other models.
 *        The function uses a 'heartbeat' mechanism to provide a smoother progress update
 *        between model steps.
 * @param step The current model step.
 * @param total_steps The total number of model steps.
 * @param nthreads The number of threads being used.
 * @return double The fraction completed (0.0 to 1.0).
 */
double model_frac_done(double step, double total_steps, int nthreads ) {
   static int     stepm1 = -1;
   static double  heartbeat = 0.0;
   static bool    debug = false;
   double         frac_done, frac_per_step;
   double         heartbeat_inc;

   frac_done     = step / total_steps;	// this increments slowly, as a model step is ~30sec->2mins cpu
   frac_per_step = 1.0 / total_steps;
   
   if (debug) {
      fprintf( stderr,"get_frac_done: step = %.0f\n", step);
      fprintf( stderr,"        total_steps = %.0f\n", total_steps );
      fprintf( stderr,"      frac_per_step = %f\n",   frac_per_step );
   }
   
   // Constant below represents estimate of how many times around the mainloop
   // before the model completes it's next step. This varies alot depending on model
   // resolution, computer speed, etc. Tune it looking at varied runtimes & resolutions!
   // Higher is better than lower to underestimate.
   // 
   // Impact of speedup due to multiple threads is accounted for below.
   //
   // If we want more accuracy could use the ratio of the model timestep to 1h (T159 tstep) to 
   // provide a 'slowdown' factor for higher resolutions.
   heartbeat_inc = (frac_per_step / (70.0 / (double)nthreads) );

   if ( (int) step > stepm1 ) {
      heartbeat = 0.0;
      stepm1 = (int) step;
   } else {
      heartbeat = heartbeat + heartbeat_inc;
      if ( heartbeat > frac_per_step )  heartbeat = frac_per_step - 0.001;  // slightly less than the next step
      frac_done = frac_done + heartbeat;
   } 

   if (frac_done < 0.0)  frac_done = 0.0;
   if (frac_done > 1.0)  frac_done = 0.9999; // never 100% until wrapper finishes
   if (debug){
      fprintf(stderr, "    heartbeat_inc = %.8f\n", heartbeat_inc);
      fprintf(stderr, "    heartbeat     = %.8f\n", heartbeat );
      double percent = frac_done * 100.0;
      fprintf(stderr, "     percent done = %.3f\n", percent);
   }

   return frac_done;
}


/**
 * @brief Moves the result file from the slot directory to the temporary project directory.
 */
int move_result_file(const std::string& slot_path, const std::string& temp_path, const std::string& result) {
    int retval = 0;

    // Move result file to the temporary folder in the project directory
    std::string result_file = slot_path + "/" + result;
    std::string temp_file = temp_path + "/" + result;

    if(file_exists(result_file)) {
       std::cerr << "Moving result file: " <<  fs::path(result_file).filename() << " to projects directory.\n";
       retval = boinc_copy( result_file.c_str(), temp_file.c_str() );

       // If result file has been successfully copied over, remove it from slots directory
       if (!retval) {
          try {
               fs::remove(result_file);
          } catch (const  fs::filesystem_error& e) {
              std::cerr << "..move_result_file(). Error removing file: " << result_file << ", error: " << e.what() << "\n";
          }
       }
    }
    return retval;
}



/**
 * @brief Takes the zip file, checks existence and whether empty and copies it to destination and unzips it
 * @param zipfile The path to the zip file containing the jf_ reference.
 * @param destination The path to copy the zip file to.
 * @param unzip_path The path to unzip the contents to.
 * @param type A string indicating the type of file (for logging purposes).
 * @return int Returns 0 on success, non-zero on failure.
 */
// GC. TODO. Convert this to accept  fs::path args.
int copy_and_unzip(const std::string& zipfile, const std::string& destination, const std::string& unzip_path, const std::string& type) {
    int retval = 0;

    // Check for the existence of the zip file
    if( !file_exists(zipfile) ) {
       std::cerr << "..The " << type << " zip file does not exist: " << zipfile << std::endl;
       return 1;        // should terminate, the model won't run.
    }

    // Check whether the zip file is empty
    if( file_is_empty(zipfile) ) {
       std::cerr << "..The " << type << " zip file is empty: " << zipfile << std::endl;
       return 1;        // should terminate, the model won't run.
    }
		
    // Get the name of the 'jf_' filename from a link within the 'zipfile' file
    std::string source = get_tag(zipfile);

    // Copy and unzip the zip file only if the zip file contains a string between tags.
    // If it doesn't, the real zip file is likely already in the working directory from a previous run.
    if ( !source.empty() ) {
       // Copy the 'jf_' file to the working directory and rename
       if ( file_exists(source) ) {
          std::cerr << "Copying the " << type << " file from: " << source << " to: " << destination << '\n';
          try {
              fs::copy_file(source, destination,  fs::copy_options::overwrite_existing);
          } 
          catch (const  fs::filesystem_error& e) {
             std::cerr << "..copy_and_unzip: Error copying file: " << source << " to: " << destination << ",\nError: " << e.what() << "\n";
             return 1;
          }
       }
       else {
          std::cerr << "..The " << type << " file retrieved from get_tag does not exist: " << source << std::endl;
          return 1;      // GC what should we do here -- return or carry on and check destination exists from a previous run?
       }
    }

    // If 'source' is empty, the 'jf_' link wasn't there so we assume the real zip file is already in the working directory.
    // We could assume that the real zip file has already been unzipped, but to be safe unzip it if found.
    if (file_exists(destination) ) {
       std::cerr << "Unzipping the " << type << " zip file: " << destination << '\n';
       if (!cpdn_unzip(destination, unzip_path)) {
         std::cerr << "..Unzipping the " << type << " file failed" << std::endl;
         return 1;
       }
    }
    else {
       std::cerr << "..The " << type << " file does not exist in the working directory: " << destination << std::endl;
       return 1;
    }

	 // Success, retval is 0
    return retval;
}


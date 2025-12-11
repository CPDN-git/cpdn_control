//
//  OpenIFS utility functions
//  To be called by Model classes for the OpenIFS based models.
//  Not to be called by the CPDN controller directly.
//
//    Glenn Carver, CPDN, 2025.

#include "oifs_utils.h"
#include "../../lib/utils.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <fstream>


// Set the required OpenIFS environment variables.
bool oifs_setenvs(const std::string& slot_path, const std::string& nthreads) {

    // For memory safety, keep env strings static so they persist for the life of the program.

    // Set the OIFS_DUMMY_ACTION environmental variable, this controls what OpenIFS does if it goes into a dummy subroutine
    // Possible values are: 'quiet', 'verbose' or 'abort'
    if ( !set_env_var("OIFS_DUMMY_ACTION", "abort") ) {
      std::cerr << "..Setting the OIFS_DUMMY_ACTION environmental variable failed" << std::endl;
      return false;
    }

    // Set the OMP_NUM_THREADS environmental variable; nthreads must be a positive integer string
    if ( !set_env_var("OMP_NUM_THREADS", nthreads) ) {
      std::cerr << "..Setting the OMP_NUM_THREADS environmental variable failed" << std::endl;
      return false;
    }
    std::cerr << "Info: OMP_NUM_THREADS is set to: " << getenv("OMP_NUM_THREADS") << "\n";

    // Set the OMP_SCHEDULE environmental variable, this enforces static thread scheduling
    if ( !set_env_var("OMP_SCHEDULE", "STATIC") ) {
      std::cerr << "..Setting the OMP_SCHEDULE environmental variable failed" << std::endl;
      return false;
    }

    // Set the DR_HOOK environmental variable, this controls the tracing facility in OpenIFS, off=0 and on=1
    if ( !set_env_var("DR_HOOK", "1") ) {
      std::cerr << "..Setting the DR_HOOK environmental variable failed" << std::endl;
      return false;
    }

    // Set the DR_HOOK_HEAPCHECK environmental variable, this ensures the heap size statistics are reported
    if ( !set_env_var("DR_HOOK_HEAPCHECK", "no") ) {
      std::cerr << "..Setting the DR_HOOK_HEAPCHECK environmental variable failed" << std::endl;
      return false;
    }

    // Set the DR_HOOK_STACKCHECK environmental variable, this ensures the stack size statistics are reported
    if ( !set_env_var("DR_HOOK_STACKCHECK", "no") ) {
      std::cerr << "..Setting the DR_HOOK_STACKCHECK environmental variable failed" << std::endl;
      return false;
    }

    // Set the EC_MEMINFO environment variable, only applies to OpenIFS 43r3.
    // Disable EC_MEMINFO to remove the useless EC_MEMINFO messages to the stdout file to reduce filesize.
    if ( !set_env_var("EC_MEMINFO", "0") ) {
       std::cerr << "..Setting the EC_MEMINFO environment variable failed" << std::endl;
       return false;
    }

    // Disable Heap memory stats at end of run; does not work for CPDN version of OpenIFS
    if ( !set_env_var("EC_PROFILE_HEAP", "0") ) {
       std::cerr << "..Setting the EC_PROFILE_HEAP environment variable failed" << std::endl;
       return false;
    }

    // Disable all memory stats at end of run; does not work for CPDN version of OpenIFS
    if ( !set_env_var("EC_PROFILE_MEM", "0") ) {
       std::cerr << "..Setting the EC_PROFILE_MEM environment variable failed" << std::endl;
       return false;
    }

    // Set the OMP_STACKSIZE environmental variable, OpenIFS needs more stack memory per process
    if ( !set_env_var("OMP_STACKSIZE", "128M") ) {
      std::cerr << "..Setting the OMP_STACKSIZE environmental variable failed" << std::endl;
      return false;
    }

    // Set the GRIB_SAMPLES_PATH environmental variable
    std::string GRIB_SAMPLES_var = slot_path + "/eccodes/ifs_samples/grib1_mlgrib2";
    if ( !set_env_var("GRIB_SAMPLES_PATH", GRIB_SAMPLES_var) )  {
      std::cerr << "..Setting the GRIB_SAMPLES_PATH failed" << std::endl;
      return false;
    }
    std::cerr << "The GRIB_SAMPLES_PATH environmental variable is: " << getenv("GRIB_SAMPLES_PATH") << "\n";

    // Set the GRIB_DEFINITION_PATH environmental variable
    std::string GRIB_DEF_var = slot_path + "/eccodes/definitions";
    if ( !set_env_var("GRIB_DEFINITION_PATH", GRIB_DEF_var) )  {
      std::cerr << "..Setting the GRIB_DEFINITION_PATH failed" << std::endl;
      return false;
    }
    std::cerr << "The GRIB_DEFINITION_PATH environmental variable is: " << getenv("GRIB_DEFINITION_PATH") << "\n";

    return true;
}



// Construct the second part of the output model filename to be uploaded
// nb. exptid is always 4 characters for OpenIFS.
std::string get_second_part(const std::string& last_iter, const std::string& exptid) {
    std::ostringstream oss;
    oss << exptid << "+" << std::setw(6) << std::setfill('0') << last_iter;
    return oss.str();
}



bool oifs_parse_stat(const std::string& logline, std::string& stat_column, const int index) {
   //   Parse a line of the OpenIFS ifs.stat log file.
   //      logline  : incoming ifs.stat logfile line to be parsed
   //      stat_col : returned string given by position 'index'
   //  Returns false if string is empty.

   std::istringstream tokens;
   std::string statstr="";

   //  split input, get token specified by 'column' unless file is corrupted
   tokens.str(logline);
   for (int i=0; i<index; ++i)
      tokens >> statstr;

   if ( statstr.empty() ){
      std::cerr << "..oifs_parse_stat: warning, statstr is empty: " << logline << '\n';
      return false;
   } else {
      stat_column = statstr;
      return true;
   }
}



bool oifs_valid_step(std::string& step, int nsteps) {
   //  checks for a valid step count in arg 'step'
   //  Returns :   true if step is valid, otherwise false
   //      Glenn

   // make sure step is valid integer
   if (!check_stoi(step)) {
      std::cerr << "..oifs_valid_step: Invalid characters in stoi string, unable to convert step to int: " << step << '\n';
      return false;
   } else {
      // check step is in valid range: 0 -> total no. of steps
      if (stoi(step)<0) {
         return false;
      } else if (stoi(step) > nsteps) {
         return false;
      }
   }
   return true;
}



/**
 * @brief Read the rcf_file line by line and extract CTIME and CSTEP variables.
 *        The input stream rcf_file must be at file start and ctime_value & cstep_value
 *        must be empty strings.
 */
bool oifs_read_rcf_file(std::ifstream& rcf_file, std::string& ctime_value, std::string& cstep_value)
{
    std::string delimiter = "\"";
    std::string rcf_file_line;
    int position = 2;

    // Extract the values of CSTEP and CTIME from the rcf file
    while ( std::getline( rcf_file, rcf_file_line ))
    {
       // Check for CSTEP, if present return value
       read_delimited_line(rcf_file_line, delimiter, "CSTEP", position, cstep_value);

       // Check for CTIME, if present return value
       read_delimited_line(rcf_file_line, delimiter, "CTIME", position, ctime_value);
    }

    if (cstep_value.empty()) {
       std::cerr << "CSTEP value not present in rcf file" << '\n';
       return false;
    } else if (ctime_value.empty()) {
       std::cerr << "CTIME value not present in rcf file" << '\n';
       return false;
    } else {
       return true;
    }
}
//
//  OpenIFS utility functions
//  To be called by Model classes for the OpenIFS based models.
//  Not to be called by the CPDN controller directly.
//
//    Glenn Carver, CPDN, 2025.


#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <fstream>
#include <vector>

#include "oifs_utils.h"
#include "../../lib/utils.h"


/**
 * @brief Set the required OpenIFS environment variables.
 *        OMP_NUM_THREADS uses the incoming nthreads value; other values are fixed.
 */
bool oifs_setenvs(const std::string& slot_path, const std::string& nthreads) {

    // check nthreads is valid integer
    // check_stoi modifies the string
    if ( std::string nthreads_copy = nthreads;
         !check_stoi(nthreads_copy) ) {
        std::cerr << "..oifs_setenvs: Invalid value of 'nthreads': " << nthreads << '\n';
        return false;
    }

    // Ordered list of environment variables to set.
    // vector of pairs is more efficient than map for single use & small number of items.
    // Use emplace_back to avoid unnecessary copies, rather than push_back.
    std::vector<std::pair<std::string, std::string>> env_vars;

    // OIFS_DUMMY_ACTION controls what the model does when it gets into a dummy subroutine.
    // Possible values are 'quiet', 'verbose' or 'abort'. We use 'abort' to stop the model.
    env_vars.emplace_back("OIFS_DUMMY_ACTION", "abort");
    env_vars.emplace_back("OMP_NUM_THREADS", nthreads);
    env_vars.emplace_back("OMP_SCHEDULE", "STATIC");    // Enforce static scheduling for OpenMP threads.
    env_vars.emplace_back("DR_HOOK", "1");              // Enable DrHook tracing; 1=on, 0=off.
    env_vars.emplace_back("DR_HOOK_HEAPCHECK", "no");   // Report heap size stats at end; yes/no.
    env_vars.emplace_back("DR_HOOK_STACKCHECK", "no");  // Report stack usage stats at end; yes/no.
    env_vars.emplace_back("EC_MEMINFO", "0");           // disable noisy EC_MEMINFO output
    env_vars.emplace_back("EC_PROFILE_HEAP", "0");      // disable heap stats; does not work with CPDN version.
    env_vars.emplace_back("EC_PROFILE_MEM", "0");       // disable memory stats; does not work with CPDN version.
    env_vars.emplace_back("OMP_STACKSIZE", "128M");     // OpenIFS needs more stack per thread

    // Paths depend on the slot directory.
    std::string grib_samples = slot_path + "/eccodes/ifs_samples/grib1_mlgrib2";
    std::string grib_defs    = slot_path + "/eccodes/definitions";
    env_vars.emplace_back("GRIB_SAMPLES_PATH", grib_samples);
    env_vars.emplace_back("GRIB_DEFINITION_PATH", grib_defs);

    for (const auto& [name, value] : env_vars) {
        if (!set_env_var(name, value)) {
            std::cerr << "..Setting the " << name << " environmental variable failed" << std::endl;
            return false;
        }

        if (name == "OMP_NUM_THREADS") {
            std::cerr << "Info: OMP_NUM_THREADS is set to: " << getenv("OMP_NUM_THREADS") << "\n";
        } else if (name == "GRIB_SAMPLES_PATH") {
            std::cerr << "The GRIB_SAMPLES_PATH environmental variable is: " << getenv("GRIB_SAMPLES_PATH") << "\n";
        } else if (name == "GRIB_DEFINITION_PATH") {
            std::cerr << "The GRIB_DEFINITION_PATH environmental variable is: " << getenv("GRIB_DEFINITION_PATH") << "\n";
        }
    }

    return true;
}



// Construct the filename part of the output model filename containing the iteration count.
// nb. exptid is always 4 characters for OpenIFS.
std::string oifs_get_filename_part(const std::string& last_iter, const std::string& exptid) {
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
    std::string rcf_file_line;
    std::string parsed_key;
    std::string parsed_value;

    // Extract the values of CSTEP and CTIME from the rcf file
    while ( std::getline( rcf_file, rcf_file_line ))
    {
       parsed_key.clear();
       parsed_value.clear();

       if (!parse_namelist_key_value(rcf_file_line, parsed_key, parsed_value)) {
          continue;
       }

       if (parsed_key == "CSTEP") {
          cstep_value = parsed_value;
       } else if (parsed_key == "CTIME") {
          ctime_value = parsed_value;
       }
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

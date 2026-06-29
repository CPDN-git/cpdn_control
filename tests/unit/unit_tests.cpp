//
// Unit tests master
//
//   Glenn Carver, CPDN, 2025

#include <functional>
#include <map>

#include "unit_tests.h"


/****************************************
  * @brief Run tests; pass test name on command line
  * 
  */
int main( int argc, char* argv[] )
{

    // Map the test name (as set in CMakeLists.txt) to the test function.
    std::map<std::string, std::function<int()>> test_map = {
        { "Read RCF File", t_read_rcf_file },
        { "Read Progress File", t_read_progress_file },
        { "CPU Time Comparison", t_cputime_comparison },
        { "Parse Key Value", t_parse_key_value },
        { "Parse Namelist Key Value", t_parse_namelist_key_value },
        { "Parse Int", t_check_parse_int },
        { "Fread Last Line", t_fread_last_line },
        { "Model Frac Done", t_model_frac_done },
        { "Path Exists", t_path_exists },
        { "File Is Empty", t_file_is_empty },
        { "Zip And Delete", t_zip_and_delete },
        { "Set Env Var", t_set_env_var },
        { "Get Out Files", t_get_out_files },
        { "Launch Process", t_launch_process },
        { "Trickle Handler", t_trickle_handler },
        { "Run Process With Timeout", t_run_process_with_timeout },
        { "Verify Project Zip MD5", t_verify_project_zip_md5 },
        { "Stage Model Input Archive", t_stage_model_input_archive },
        { "Parse Control Input", t_parse_control_input },
        { "Control Start", t_control_start },
        { "Logging Utils", t_logging_utils },
        { "Get Env Vars", t_get_env_vars },
        { "Model Filename Match", t_model_filename_match },
        { "OpenIFS Current Step", t_oifs_current_step },
        { "WRF Datetime", t_wrf_datetime },
        { "WRF Current Step", t_wrf_current_step },
        { "WRF Parse Restart", t_wrf_parse_restart },
        { "WRF Check Model Success", t_wrf_check_model_success },
        { "WRF Restart Pruning", t_wrf_restart_pruning },
        // Add new test functions here! Remember previous trailing comma!
    };

    if ( argc != 2 ) {
        std::cerr << "ERROR: Usage:  <TestName>" << std::endl;
        std::cerr << "See code or CMakeLists.txt for test names." << std::endl;
        return EXIT_FAILURE;    // Returning failure here is useful if CTest misconfigures the call
    }

    std::string test_name = argv[1];
    auto t_fn = test_map.find( test_name );

    if ( t_fn == test_map.end() ) {
        std::cerr << "ERROR: Test '" << test_name << "' not found." << std::endl;
        return EXIT_FAILURE;
    }

    // Run the required test
    std::cout << " ---- Running Unit Test : " << test_name << " ----\n" << std::endl;
    auto ret = t_fn->second();

    return ret;    // must be either EXIT_FAILURE or EXIT_SUCCESS.
}

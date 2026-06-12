/**
 * @file t_get_env_vars.cpp
 * @brief Unit test for model-specific get_env_vars() overrides.
 */

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "../models/openifs/oifs_control.h"
#include "../models/wrf/wrf_control.h"
#include "unit_tests.h"

namespace {

bool contains_entry( const std::vector<std::string>& env_vars, const std::string& expected_entry )
{
    for ( const auto& env_var : env_vars ) {
        if ( env_var == expected_entry ) {
            return true;
        }
    }
    return false;
}

}    // namespace

int t_get_env_vars()
{
    TEST( "t_get_env_vars" );

    int test_count = 0;
    int test_passed = 0;

    OpenIFSControl openifs_model( "ECMWF", "oifs_43r3_omp_l159", "1.0.0", "oifs_43r3_omp_model.exe" );
    WRFControl wrf_model( "UCAR", "wrf_4.6.1_urban", "4.6.1", "wrf_4.6.1_urban.exe" );

    std::string err_msg;

    test_count++;
    auto openifs_env_vars = openifs_model.get_env_vars( "/tmp/oifs_slot", "4", err_msg );
    if ( err_msg.empty() && contains_entry( openifs_env_vars, "OIFS_DUMMY_ACTION=abort" ) &&
         contains_entry( openifs_env_vars, "GRIB_SAMPLES_PATH=/tmp/oifs_slot/eccodes/ifs_samples/grib1_mlgrib2" ) &&
         contains_entry( openifs_env_vars, "GRIB_DEFINITION_PATH=/tmp/oifs_slot/eccodes/definitions" ) &&
         contains_entry( openifs_env_vars, "OMP_NUM_THREADS=4" ) && contains_entry( openifs_env_vars, "OMP_SCHEDULE=STATIC" ) &&
         contains_entry( openifs_env_vars, "OMP_STACKSIZE=128M" ) ) {
        test_passed++;
    } else {
        std::cerr << "  Expected OpenIFS environment entries, err_msg='" << err_msg << "'\n";
    }

    test_count++;
    err_msg.clear();
    openifs_env_vars = openifs_model.get_env_vars( "/tmp/oifs_slot", "bad", err_msg );
    if ( openifs_env_vars.empty() && err_msg == "invalid value of 'nthreads': bad" ) {
        test_passed++;
    } else {
        std::cerr << "  Expected OpenIFS invalid nthreads failure, err_msg='" << err_msg << "'\n";
    }

    test_count++;
    err_msg.clear();
    auto wrf_env_vars = wrf_model.get_env_vars( "/tmp/wrf_slot", "8", err_msg );
    if ( err_msg.empty() && wrf_env_vars.size() == 1 && wrf_env_vars.front() == "OMP_NUM_THREADS=8" ) {
        test_passed++;
    } else {
        std::cerr << "  Expected WRF OMP_NUM_THREADS entry, err_msg='" << err_msg << "'\n";
    }

    test_count++;
    err_msg.clear();
    wrf_env_vars = wrf_model.get_env_vars( "/tmp/wrf_slot", "oops", err_msg );
    if ( wrf_env_vars.empty() && err_msg == "invalid value of 'nthreads': oops" ) {
        test_passed++;
    } else {
        std::cerr << "  Expected WRF invalid nthreads failure, err_msg='" << err_msg << "'\n";
    }

    std::cout << "  get_env_vars: " << test_passed << "/" << test_count << " tests passed\n";
    if ( test_passed == test_count ) {
        TEST_SUCCESS;
        return EXIT_SUCCESS;
    } else {
        TEST_FAIL;
        return EXIT_FAILURE;
    }
}

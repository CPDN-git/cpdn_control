// Test model-owned filename matching helpers.
//
//  Glenn Carver, CPDN, 2026

#include <cstdlib>
#include <iostream>
#include <string>

#include "../models/openifs/oifs_control.h"
#include "../models/wrf/wrf_control.h"
#include "unit_tests.h"

int t_model_filename_match()
{
    TEST( "t_model_filename_match" );

    int test_count = 0;
    int test_passed = 0;

    OpenIFSControl openifs_model( "ECMWF", "oifs_43r3_omp_l159", "1.0.0", "oifs_43r3_omp_model.exe" );
    WRFControl wrf_model( "UCAR", "wrf_4.6.1_urban", "4.6.1", "wrf_4.6.1_urban.exe" );

    test_count++;
    if ( openifs_model.is_output_filename( "ICMSHABCD+000123" ) && openifs_model.is_output_filename( "ICMGGABCD+999999" ) &&
         !openifs_model.is_output_filename( "ICMSHAB1D+000123" ) && !openifs_model.is_output_filename( "ICMSHABCD-000123" ) ) {
        test_passed++;
    } else {
        std::cerr << "  OpenIFS output filename matching did not behave as expected\n";
    }

    test_count++;
    if ( openifs_model.is_restart_filename( "rcf" ) && !openifs_model.is_restart_filename( "rcf.tmp" ) ) {
        test_passed++;
    } else {
        std::cerr << "  OpenIFS restart filename matching did not behave as expected\n";
    }

    test_count++;
    if ( wrf_model.is_output_filename( "wrfout_d03_2022-07-01_00:00:00" ) &&
         !wrf_model.is_output_filename( "wrfout_d03_2022/07/01_00:00:00" ) &&
         !wrf_model.is_output_filename( "wrfout_d03_2022-07-01-00:00:00" ) ) {
        test_passed++;
    } else {
        std::cerr << "  WRF output filename matching did not behave as expected\n";
    }

    test_count++;
    if ( wrf_model.is_restart_filename( "wrfrst_d03_2022-07-01_00:00:00" ) &&
         !wrf_model.is_restart_filename( "wrfrst_d03_2022-07-01_00-00-00" ) ) {
        test_passed++;
    } else {
        std::cerr << "  WRF restart filename matching did not behave as expected\n";
    }

    std::cout << "  model_filename_match: " << test_passed << "/" << test_count << " tests passed\n";
    if ( test_passed == test_count ) {
        TEST_SUCCESS;
        return EXIT_SUCCESS;
    }

    TEST_FAIL;
    return EXIT_FAILURE;
}

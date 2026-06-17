// Test model-owned filename matching helpers.
//
//  Glenn Carver, CPDN, 2026

#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "../models/openifs/oifs_control.h"
#include "../models/wrf/wrf_control.h"
#include "unit_tests.h"

namespace fs = std::filesystem;

namespace {

bool write_text_file( const fs::path& path, const std::string& content )
{
    std::ofstream out( path, std::ios::out | std::ios::trunc );
    if ( !out.is_open() ) {
        return false;
    }

    out << content;
    return static_cast<bool>( out );
}

}    // namespace

int t_model_filename_match()
{
    TEST( "t_model_filename_match" );

    int test_count = 0;
    int test_passed = 0;

    const fs::path original_cwd = fs::current_path();
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path tmp_dir = fs::temp_directory_path() / ( "cpdn_wrf_filename_match_" + std::to_string( now ) );
    std::error_code ec;
    fs::create_directories( tmp_dir, ec );
    if ( ec ) {
        TEST_FAIL;
        std::cout << "Unable to create temp dir: " << tmp_dir.string() << "\n";
        return EXIT_FAILURE;
    }

    OpenIFSControl openifs_model( "ECMWF", "oifs_43r3_omp_l159", "1.0.0", "oifs_43r3_omp_model.exe" );
    WRFControl wrf_model( "UCAR", "wrf_4.6.1_urban", "4.6.1", "wrf_4.6.1_urban.exe" );

    fs::current_path( tmp_dir );
    const std::string wrf_content = "&time_control\n"
                                    " run_days = 0,\n"
                                    " run_hours = 1,\n"
                                    " run_minutes = 0,\n"
                                    " run_seconds = 0,\n"
                                    " history_interval = 9999, 9999, 60,\n"
                                    " frames_per_outfile = 1, 1, 24,\n"
                                    " restart_interval = 180,\n"
                                    "/\n"
                                    "&domains\n"
                                    " time_step = 300,\n"
                                    " max_dom = 3,\n"
                                    "/\n";
    if ( !write_text_file( tmp_dir / "namelist.input", wrf_content ) ) {
        TEST_FAIL;
        std::cout << "Unable to write WRF namelist.input test file\n";
        fs::current_path( original_cwd );
        fs::remove_all( tmp_dir, ec );
        return EXIT_FAILURE;
    }

    const auto parsed = wrf_model.parse_control_input();
    if ( !parsed.ok ) {
        TEST_FAIL;
        std::cout << "Failed to parse WRF namelist.input for filename match test\n";
        fs::current_path( original_cwd );
        fs::remove_all( tmp_dir, ec );
        return EXIT_FAILURE;
    }

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
    if ( wrf_model.is_output_filename( "wrfout_d01_2022-07-01_00:00:00" ) &&
         wrf_model.is_output_filename( "wrfout_d02_2022-07-01_00:00:00" ) &&
         wrf_model.is_output_filename( "wrfout_d03_2022-07-01_00:00:00" ) &&
         !wrf_model.is_output_filename( "wrfout_d03_2022/07/01_00:00:00" ) &&
         !wrf_model.is_output_filename( "wrfout_d04_2022-07-01_00:00:00" ) &&
         !wrf_model.is_output_filename( "wrfout_d03_2022-07-01-00:00:00" ) ) {
        test_passed++;
    } else {
        std::cerr << "  WRF output filename matching did not behave as expected\n";
    }

    test_count++;
    if ( wrf_model.is_restart_filename( "wrfrst_d01_2022-07-01_00:00:00" ) &&
         wrf_model.is_restart_filename( "wrfrst_d02_2022-07-01_00:00:00" ) &&
         wrf_model.is_restart_filename( "wrfrst_d03_2022-07-01_00:00:00" ) &&
         !wrf_model.is_restart_filename( "wrfrst_d04_2022-07-01_00:00:00" ) &&
         !wrf_model.is_restart_filename( "wrfrst_d03_2022-07-01_00-00-00" ) ) {
        test_passed++;
    } else {
        std::cerr << "  WRF restart filename matching did not behave as expected\n";
    }

    std::cout << "  model_filename_match: " << test_passed << "/" << test_count << " tests passed\n";
    fs::current_path( original_cwd );
    fs::remove_all( tmp_dir, ec );
    if ( test_passed == test_count ) {
        TEST_SUCCESS;
        return EXIT_SUCCESS;
    }

    TEST_FAIL;
    return EXIT_FAILURE;
}

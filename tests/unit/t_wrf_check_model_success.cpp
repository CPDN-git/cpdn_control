// Unit tests for WRF success-marker detection in stderr output.
//
//  Glenn Carver, CPDN, 2026

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

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

int t_wrf_check_model_success()
{
    TEST( "t_wrf_check_model_success" );

    const fs::path original_cwd = fs::current_path();
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path tmp_dir = fs::temp_directory_path() / ( "cpdn_wrf_check_model_success_" + std::to_string( now ) );

    std::error_code ec;
    fs::create_directories( tmp_dir, ec );
    if ( ec ) {
        TEST_FAIL;
        std::cout << "Unable to create temp dir: " << tmp_dir.string() << "\n";
        return EXIT_FAILURE;
    }

    fs::current_path( tmp_dir );

    int test_count = 0;
    int test_passed = 0;

    WRFControl wrf_model( "UCAR", "wrf_4.6.1_urban", "4.6.1", "wrf_4.6.1_urban.exe" );

    test_count++;
    const std::string success_not_last_content = "Starting WRF run\n"
                                                 "SUCCESS COMPLETE WRF\n"
                                                 "Post-processing summary line\n";
    if ( write_text_file( tmp_dir / "stderr.txt", success_not_last_content ) && wrf_model.check_model_success() ) {
        test_passed++;
    } else {
        std::cout << "Expected success when success marker is present before the final line\n";
    }

    test_count++;
    const std::string missing_success_content = "Starting WRF run\n"
                                                "Intermediate output\n"
                                                "Post-processing summary line\n";
    if ( write_text_file( tmp_dir / "stderr.txt", missing_success_content ) && !wrf_model.check_model_success() ) {
        test_passed++;
    } else {
        std::cout << "Expected failure when success marker is absent\n";
    }

    std::cout << "  wrf_check_model_success: " << test_passed << "/" << test_count << " tests passed\n";

    fs::current_path( original_cwd );
    fs::remove_all( tmp_dir, ec );

    if ( test_passed == test_count ) {
        TEST_SUCCESS;
        return EXIT_SUCCESS;
    }

    TEST_FAIL;
    return EXIT_FAILURE;
}

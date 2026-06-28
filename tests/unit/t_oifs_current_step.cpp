// Unit tests for OpenIFS current-step parsing from ifs.stat.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "../models/openifs/oifs_control.h"
#include "unit_tests.h"

namespace fs = std::filesystem;

namespace {

bool write_text_file( const fs::path& path, const std::string& content )
{
    std::ofstream out( path );
    if ( !out.is_open() ) {
        return false;
    }
    out << content;
    return static_cast<bool>( out );
}

}    // namespace

int t_oifs_current_step()
{
    TEST( "t_oifs_current_step" );

    int test_count = 0;
    int test_passed = 0;

    const fs::path original_cwd = fs::current_path();
    fs::path tmp_dir = fs::temp_directory_path() / "cpdn_oifs_current_step_test";

    std::error_code ec;
    fs::remove_all( tmp_dir, ec );
    fs::create_directories( tmp_dir, ec );
    fs::current_path( tmp_dir );

    OpenIFSControl oifs_model( "ECMWF", "oifs_43r3_omp_l159", "1.0.0", "oifs_43r3_omp_model.exe" );

    test_count++;
    {
        const std::string valid_content = " 12:00:00 0AAA00AAA STEPO                      24\n";
        int step = 0;
        if ( write_text_file( tmp_dir / "ifs.stat", valid_content ) && oifs_model.get_current_step( step, 24 ) && step == 24 ) {
            test_passed++;
        } else {
            std::cout << "Expected valid OpenIFS step 24 from ifs.stat, got " << step << "\n";
        }
    }

    test_count++;
    {
        const std::string invalid_final_content = " 12:00:00 0AAA00AAA STEPO                      24\n"
                                                  " 12:00:01 0AAA00AAA CNT0                      25\n";
        int step = 24;
        if ( write_text_file( tmp_dir / "ifs.stat", invalid_final_content ) && !oifs_model.get_current_step( step, 24 ) && step == 24 ) {
            test_passed++;
        } else {
            std::cout << "Expected failed OpenIFS step read to preserve caller step 24, got " << step << "\n";
        }
    }

    fs::current_path( original_cwd );
    fs::remove_all( tmp_dir, ec );

    std::cout << "  oifs_current_step: " << test_passed << "/" << test_count << " tests passed\n";
    if ( test_passed == test_count ) {
        TEST_SUCCESS;
        return EXIT_SUCCESS;
    }

    TEST_FAIL;
    return EXIT_FAILURE;
}

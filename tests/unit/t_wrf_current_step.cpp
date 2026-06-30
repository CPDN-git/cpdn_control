// Unit tests for WRF current-step extraction from stdout timing output.
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

int t_wrf_current_step()
{
    TEST( "t_wrf_current_step" );

    const fs::path original_cwd = fs::current_path();
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path tmp_dir = fs::temp_directory_path() / ( "cpdn_wrf_current_step_" + std::to_string( now ) );

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
    const std::string wrf_content = "&time_control\n"
                                    " run_days = 0,\n"
                                    " run_hours = 0,\n"
                                    " run_minutes = 1,\n"
                                    " run_seconds = 0,\n"
                                    " start_year = 2022, 2022, 2022,\n"
                                    " start_month = 7, 7, 7,\n"
                                    " start_day = 1, 1, 1,\n"
                                    " start_hour = 0, 0, 0,\n"
                                    " start_minute = 50, 50, 50,\n"
                                    " start_second = 0, 0, 0,\n"
                                    " history_interval = 9999, 9999, 60,\n"
                                    " frames_per_outfile = 1, 1, 24,\n"
                                    " restart_interval = 180,\n"
                                    "/\n"
                                    "&domains\n"
                                    " time_step = 5,\n"
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
        std::cout << "Failed to parse WRF namelist.input in current-step test\n";
        fs::current_path( original_cwd );
        fs::remove_all( tmp_dir, ec );
        return EXIT_FAILURE;
    }

    test_count++;
    const std::string stdout_content = " CPDN DEBUG: calling cpdn_checkpid for grid%id = 1\n"
                                       " read progfile ok: control_pid =        71980\n"
                                       "Timing for main: time 2022-07-01_00:50:05 on domain   3:   11.33282 elapsed seconds\n"
                                       "Timing for main: time 2022-07-01_00:50:15 on domain   2:   26.40792 elapsed seconds\n"
                                       "Timing for main: time 2022-07-01_00:51:00 on domain   1:   68.27209 elapsed seconds\n"
                                       "Timing for main: time 2022-07-01_00:51:05 on domain   3:    2.11111 elapsed seconds\n";

    if ( !write_text_file( tmp_dir / "stdout.txt", stdout_content ) ) {
        TEST_FAIL;
        std::cout << "Unable to write WRF stdout.txt test file\n";
        fs::current_path( original_cwd );
        fs::remove_all( tmp_dir, ec );
        return EXIT_FAILURE;
    }

    int step = -1;
    if ( wrf_model.get_current_step( step, parsed.total_steps ) && step == 12 ) {
        test_passed++;
    } else {
        std::cout << "Expected current step 12, got " << step << "\n";
    }

    test_count++;
    if ( !write_text_file( tmp_dir / "stdout.txt", "Timing for main: time 2022-07-01_00:50:05 on domain   3:   11.33282 elapsed seconds\n" ) ) {
        TEST_FAIL;
        std::cout << "Unable to rewrite WRF stdout.txt test file\n";
        fs::current_path( original_cwd );
        fs::remove_all( tmp_dir, ec );
        return EXIT_FAILURE;
    }

    step = -1;
    if ( !wrf_model.get_current_step( step, parsed.total_steps ) ) {
        test_passed++;
    } else {
        std::cout << "Expected get_current_step() failure when no domain 1 timing line exists, got step " << step << "\n";
    }

    std::cout << "  wrf_current_step: " << test_passed << "/" << test_count << " tests passed\n";

    fs::current_path( original_cwd );
    fs::remove_all( tmp_dir, ec );

    if ( test_passed == test_count ) {
        TEST_SUCCESS;
        return EXIT_SUCCESS;
    }

    TEST_FAIL;
    return EXIT_FAILURE;
}

// Unit tests for WRF restart-file pruning during step tasks.
//
//  Glenn Carver, CPDN, 2026

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

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

bool write_binary_stub( const fs::path& path )
{
    std::ofstream out( path, std::ios::out | std::ios::binary | std::ios::trunc );
    if ( !out.is_open() ) {
        return false;
    }

    out << "restart-data";
    return static_cast<bool>( out );
}

std::string make_platform_restart_filename( std::string filename )
{
#if defined( _WIN32 )
    std::replace( filename.begin(), filename.end(), ':', '-' );
#endif
    return filename;
}

bool all_files_exist( const fs::path& dir, const std::vector<std::string>& filenames )
{
    for ( const auto& filename : filenames ) {
        if ( !fs::exists( dir / filename ) ) {
            return false;
        }
    }

    return true;
}

bool no_files_exist( const fs::path& dir, const std::vector<std::string>& filenames )
{
    for ( const auto& filename : filenames ) {
        if ( fs::exists( dir / filename ) ) {
            return false;
        }
    }

    return true;
}

}    // namespace

int t_wrf_restart_pruning()
{
    TEST( "t_wrf_restart_pruning" );

    const fs::path original_cwd = fs::current_path();
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path tmp_dir = fs::temp_directory_path() / ( "cpdn_wrf_restart_pruning_" + std::to_string( now ) );

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
        std::cout << "Failed to parse WRF namelist.input for restart pruning test\n";
        fs::current_path( original_cwd );
        fs::remove_all( tmp_dir, ec );
        return EXIT_FAILURE;
    }

    const std::vector<std::string> oldest_files = {
        make_platform_restart_filename( "wrfrst_d01_2022-07-01_18:00:00" ),
        make_platform_restart_filename( "wrfrst_d02_2022-07-01_18:00:00" ),
        make_platform_restart_filename( "wrfrst_d03_2022-07-01_18:00:00" ),
    };
    const std::vector<std::string> middle_files = {
        make_platform_restart_filename( "wrfrst_d01_2022-07-01_20:00:00" ),
        make_platform_restart_filename( "wrfrst_d02_2022-07-01_20:00:00" ),
        make_platform_restart_filename( "wrfrst_d03_2022-07-01_20:00:00" ),
    };
    const std::vector<std::string> newest_files = {
        make_platform_restart_filename( "wrfrst_d01_2022-07-01_22:00:00" ),
        make_platform_restart_filename( "wrfrst_d02_2022-07-01_22:00:00" ),
        make_platform_restart_filename( "wrfrst_d03_2022-07-01_22:00:00" ),
    };

    for ( const auto& filename : oldest_files ) {
        if ( !write_binary_stub( tmp_dir / filename ) ) {
            TEST_FAIL;
            std::cout << "Unable to write oldest restart test file: " << filename << "\n";
            fs::current_path( original_cwd );
            fs::remove_all( tmp_dir, ec );
            return EXIT_FAILURE;
        }
    }
    for ( const auto& filename : middle_files ) {
        if ( !write_binary_stub( tmp_dir / filename ) ) {
            TEST_FAIL;
            std::cout << "Unable to write middle restart test file: " << filename << "\n";
            fs::current_path( original_cwd );
            fs::remove_all( tmp_dir, ec );
            return EXIT_FAILURE;
        }
    }
    for ( const auto& filename : newest_files ) {
        if ( !write_binary_stub( tmp_dir / filename ) ) {
            TEST_FAIL;
            std::cout << "Unable to write newest restart test file: " << filename << "\n";
            fs::current_path( original_cwd );
            fs::remove_all( tmp_dir, ec );
            return EXIT_FAILURE;
        }
    }

    test_count++;
    if ( wrf_model.do_step_tasks( 23, tmp_dir ) && all_files_exist( tmp_dir, oldest_files ) && all_files_exist( tmp_dir, middle_files ) &&
         all_files_exist( tmp_dir, newest_files ) ) {
        test_passed++;
    } else {
        std::cout << "Expected no pruning when current_step is not a multiple of 24\n";
    }

    test_count++;
    if ( wrf_model.do_step_tasks( 24, tmp_dir ) && no_files_exist( tmp_dir, oldest_files ) && all_files_exist( tmp_dir, middle_files ) &&
         all_files_exist( tmp_dir, newest_files ) ) {
        test_passed++;
    } else {
        std::cout << "Expected pruning to keep only the two newest restart timestamps\n";
    }

    std::cout << "  wrf_restart_pruning: " << test_passed << "/" << test_count << " tests passed\n";

    fs::current_path( original_cwd );
    fs::remove_all( tmp_dir, ec );

    if ( test_passed == test_count ) {
        TEST_SUCCESS;
        return EXIT_SUCCESS;
    }

    TEST_FAIL;
    return EXIT_FAILURE;
}

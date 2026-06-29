// Unit tests for WRF restart-state parsing and setup-time namelist updates.
//
//  Glenn Carver, CPDN, 2026

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

bool read_text_file( const fs::path& path, std::string& content )
{
    std::ifstream in( path );
    if ( !in.is_open() ) {
        return false;
    }

    content.assign( std::istreambuf_iterator<char>( in ), std::istreambuf_iterator<char>() );
    return static_cast<bool>( in ) || in.eof();
}

bool remove_files( const fs::path& dir, const std::vector<std::string>& filenames )
{
    std::error_code ec;
    for ( const auto& filename : filenames ) {
        if ( !fs::remove( dir / filename, ec ) || ec ) {
            return false;
        }
    }

    return true;
}

}    // namespace

int t_wrf_parse_restart()
{
    TEST( "t_wrf_parse_restart" );

    const fs::path original_cwd = fs::current_path();
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path tmp_root = fs::temp_directory_path() / ( "cpdn_wrf_parse_restart_" + std::to_string( now ) );

    std::error_code ec;
    fs::create_directories( tmp_root, ec );
    if ( ec ) {
        TEST_FAIL;
        std::cout << "Unable to create temp dir: " << tmp_root.string() << "\n";
        return EXIT_FAILURE;
    }

    int test_count = 0;
    int test_passed = 0;

    const std::string wrf_namelist_with_restart_false = "&time_control\n"
                                                        " restart = .false.,\n"
                                                        " run_days = 0,\n"
                                                        " run_hours = 1,\n"
                                                        " run_minutes = 0,\n"
                                                        " run_seconds = 0,\n"
                                                        " start_year = 2022, 2022, 2022,\n"
                                                        " start_month = 7, 7, 7,\n"
                                                        " start_day = 1, 1, 1,\n"
                                                        " start_hour = 0, 0, 0,\n"
                                                        " start_minute = 0, 0, 0,\n"
                                                        " start_second = 0, 0, 0,\n"
                                                        " history_interval = 9999, 9999, 60,\n"
                                                        " frames_per_outfile = 1, 1, 24,\n"
                                                        " restart_interval = 180,\n"
                                                        "/\n"
                                                        "&domains\n"
                                                        " time_step = 300,\n"
                                                        " max_dom = 3,\n"
                                                        "/\n";

    const std::string wrf_namelist_without_restart_key = "&time_control\n"
                                                         " run_days = 0,\n"
                                                         " run_hours = 1,\n"
                                                         " run_minutes = 0,\n"
                                                         " run_seconds = 0,\n"
                                                         " start_year = 2022, 2022, 2022,\n"
                                                         " start_month = 7, 7, 7,\n"
                                                         " start_day = 1, 1, 1,\n"
                                                         " start_hour = 0, 0, 0,\n"
                                                         " start_minute = 0, 0, 0,\n"
                                                         " start_second = 0, 0, 0,\n"
                                                         " history_interval = 9999, 9999, 60,\n"
                                                         " frames_per_outfile = 1, 1, 24,\n"
                                                         " restart_interval = 180,\n"
                                                         "/\n"
                                                         "&domains\n"
                                                         " time_step = 300,\n"
                                                         " max_dom = 3,\n"
                                                         "/\n";

    {
        std::cout << "Subtest: valid restart set rewrites restart flag and parse_restart uses cached scan state\n";
        const fs::path tmp_dir = tmp_root / "rewrite_existing_key";
        fs::create_directories( tmp_dir, ec );
        if ( ec ) {
            TEST_FAIL;
            std::cout << "Unable to create subdir: " << tmp_dir.string() << "\n";
            fs::remove_all( tmp_root, ec );
            return EXIT_FAILURE;
        }

        fs::current_path( tmp_dir );
        WRFControl wrf_model( "UCAR", "wrf_4.6.1_urban", "4.6.1", "wrf_4.6.1_urban.exe" );

        if ( !write_text_file( tmp_dir / "namelist.input", wrf_namelist_with_restart_false ) ) {
            TEST_FAIL;
            std::cout << "Unable to write WRF namelist.input test file\n";
            fs::current_path( original_cwd );
            fs::remove_all( tmp_root, ec );
            return EXIT_FAILURE;
        }

        const std::vector<std::string> valid_restart_files = {
            "wrfrst_d01_2022-07-01_00:15:00",
            "wrfrst_d02_2022-07-01_00:15:00",
            "wrfrst_d03_2022-07-01_00:15:00",
        };
        const std::vector<std::string> incomplete_restart_files = {
            "wrfrst_d01_2022-07-01_00:20:00",
            "wrfrst_d02_2022-07-01_00:20:00",
        };

        for ( const auto& filename : valid_restart_files ) {
            if ( !write_binary_stub( tmp_dir / filename ) ) {
                TEST_FAIL;
                std::cout << "Unable to write valid restart test file: " << filename << "\n";
                fs::current_path( original_cwd );
                fs::remove_all( tmp_root, ec );
                return EXIT_FAILURE;
            }
        }
        for ( const auto& filename : incomplete_restart_files ) {
            if ( !write_binary_stub( tmp_dir / filename ) ) {
                TEST_FAIL;
                std::cout << "Unable to write incomplete restart test file: " << filename << "\n";
                fs::current_path( original_cwd );
                fs::remove_all( tmp_root, ec );
                return EXIT_FAILURE;
            }
        }

        test_count++;
        std::string namelist_content;
        if ( wrf_model.setup( tmp_dir ) && read_text_file( tmp_dir / "namelist.input", namelist_content ) &&
             namelist_content.find( "restart = .true." ) != std::string::npos ) {
            test_passed++;
        } else {
            std::cout << "Expected setup() to enable restart in namelist.input\n";
        }

        const auto parsed = wrf_model.parse_control_input();
        if ( !parsed.ok ) {
            TEST_FAIL;
            std::cout << "Failed to parse WRF namelist.input in parse_restart test\n";
            fs::current_path( original_cwd );
            fs::remove_all( tmp_root, ec );
            return EXIT_FAILURE;
        }

        test_count++;
        std::string restart_step;
        if ( remove_files( tmp_dir, valid_restart_files ) && remove_files( tmp_dir, incomplete_restart_files ) && wrf_model.restart_exists() &&
             wrf_model.parse_restart( restart_step ) && restart_step == "3" ) {
            test_passed++;
        } else {
            std::cout << "Expected cached restart scan to survive file removal and map latest valid restart to step 3. Got step=" << restart_step << "\n";
        }
    }

    {
        std::cout << "Subtest: missing restart key is inserted directly after &time_control\n";
        const fs::path tmp_dir = tmp_root / "insert_missing_key";
        ec.clear();
        fs::create_directories( tmp_dir, ec );
        if ( ec ) {
            TEST_FAIL;
            std::cout << "Unable to create subdir: " << tmp_dir.string() << "\n";
            fs::current_path( original_cwd );
            fs::remove_all( tmp_root, ec );
            return EXIT_FAILURE;
        }

        fs::current_path( tmp_dir );
        WRFControl wrf_model( "UCAR", "wrf_4.6.1_urban", "4.6.1", "wrf_4.6.1_urban.exe" );

        if ( !write_text_file( tmp_dir / "namelist.input", wrf_namelist_without_restart_key ) ) {
            TEST_FAIL;
            std::cout << "Unable to write WRF namelist.input missing-key test file\n";
            fs::current_path( original_cwd );
            fs::remove_all( tmp_root, ec );
            return EXIT_FAILURE;
        }

        const std::vector<std::string> restart_files = {
            "wrfrst_d01_2022-07-01_00:10:00",
            "wrfrst_d02_2022-07-01_00:10:00",
            "wrfrst_d03_2022-07-01_00:10:00",
        };
        for ( const auto& filename : restart_files ) {
            if ( !write_binary_stub( tmp_dir / filename ) ) {
                TEST_FAIL;
                std::cout << "Unable to write restart test file: " << filename << "\n";
                fs::current_path( original_cwd );
                fs::remove_all( tmp_root, ec );
                return EXIT_FAILURE;
            }
        }

        test_count++;
        std::string namelist_content;
        if ( wrf_model.setup( tmp_dir ) && read_text_file( tmp_dir / "namelist.input", namelist_content ) &&
             namelist_content.find( "&time_control\n restart = .true.," ) != std::string::npos ) {
            test_passed++;
        } else {
            std::cout << "Expected setup() to insert restart = .true. directly after &time_control\n";
        }

        const auto parsed = wrf_model.parse_control_input();
        if ( !parsed.ok ) {
            TEST_FAIL;
            std::cout << "Failed to parse WRF namelist.input after restart key insertion\n";
            fs::current_path( original_cwd );
            fs::remove_all( tmp_root, ec );
            return EXIT_FAILURE;
        }

        test_count++;
        std::string restart_step;
        if ( wrf_model.parse_restart( restart_step ) && restart_step == "2" ) {
            test_passed++;
        } else {
            std::cout << "Expected inserted-key restart parse to return step 2. Got step=" << restart_step << "\n";
        }
    }

    {
        std::cout << "Subtest: incomplete restart set does not change namelist and parse_restart fails\n";
        const fs::path tmp_dir = tmp_root / "incomplete_restart";
        ec.clear();
        fs::create_directories( tmp_dir, ec );
        if ( ec ) {
            TEST_FAIL;
            std::cout << "Unable to create subdir: " << tmp_dir.string() << "\n";
            fs::current_path( original_cwd );
            fs::remove_all( tmp_root, ec );
            return EXIT_FAILURE;
        }

        fs::current_path( tmp_dir );
        WRFControl wrf_model( "UCAR", "wrf_4.6.1_urban", "4.6.1", "wrf_4.6.1_urban.exe" );

        if ( !write_text_file( tmp_dir / "namelist.input", wrf_namelist_with_restart_false ) ) {
            TEST_FAIL;
            std::cout << "Unable to write WRF namelist.input incomplete-restart test file\n";
            fs::current_path( original_cwd );
            fs::remove_all( tmp_root, ec );
            return EXIT_FAILURE;
        }

        const std::vector<std::string> incomplete_restart_files = {
            "wrfrst_d01_2022-07-01_00:25:00",
            "wrfrst_d02_2022-07-01_00:25:00",
        };
        for ( const auto& filename : incomplete_restart_files ) {
            if ( !write_binary_stub( tmp_dir / filename ) ) {
                TEST_FAIL;
                std::cout << "Unable to write incomplete restart test file: " << filename << "\n";
                fs::current_path( original_cwd );
                fs::remove_all( tmp_root, ec );
                return EXIT_FAILURE;
            }
        }

        test_count++;
        std::string namelist_content;
        if ( wrf_model.setup( tmp_dir ) && read_text_file( tmp_dir / "namelist.input", namelist_content ) &&
             namelist_content.find( "restart = .false." ) != std::string::npos && namelist_content.find( "restart = .true." ) == std::string::npos ) {
            test_passed++;
        } else {
            std::cout << "Expected incomplete restart set to leave namelist.input unchanged\n";
        }

        const auto parsed = wrf_model.parse_control_input();
        if ( !parsed.ok ) {
            TEST_FAIL;
            std::cout << "Failed to parse WRF namelist.input for incomplete restart test\n";
            fs::current_path( original_cwd );
            fs::remove_all( tmp_root, ec );
            return EXIT_FAILURE;
        }

        test_count++;
        std::string restart_step;
        if ( !wrf_model.restart_exists() && !wrf_model.parse_restart( restart_step ) ) {
            test_passed++;
        } else {
            std::cout << "Expected incomplete restart set to keep restart_exists()/parse_restart() false. Got step=" << restart_step << "\n";
        }
    }

    std::cout << "  wrf_parse_restart: " << test_passed << "/" << test_count << " tests passed\n";

    fs::current_path( original_cwd );
    fs::remove_all( tmp_root, ec );

    if ( test_passed == test_count ) {
        TEST_SUCCESS;
        return EXIT_SUCCESS;
    }

    TEST_FAIL;
    return EXIT_FAILURE;
}

/**
 * @file t_stage_model_input_archive.cpp
 * @brief Unit test for stage_model_input_archive()
 */

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "boinc/md5_file.h"

#include "../src/cpdn_control.h"
#include "../zip/cpdn_zip.h"
#include "unit_tests.h"

namespace fs = std::filesystem;

static bool create_md5_named_zip( const fs::path& work_dir, const std::string& payload_name, const std::string& content, fs::path& archive_path )
{
    fs::path payload = work_dir / payload_name;
    std::ofstream file( payload );
    file << content;
    file.close();

    fs::path zip_path = work_dir / ( payload_name + ".zip" );
    if ( !cpdn_zip( zip_path, { payload } ) ) {
        return false;
    }

    fs::remove( payload );

    char md5_buf[MD5_LEN] = { 0 };
    double nbytes = 0.0;
    if ( md5_file( zip_path.string().c_str(), md5_buf, nbytes ) != 0 ) {
        return false;
    }

    archive_path = work_dir / ( "jf_" + std::string( md5_buf ) );
    fs::rename( zip_path, archive_path );
    return true;
}

int t_stage_model_input_archive()
{
    TEST( "t_stage_model_input_archive" );

    int test_count = 0;
    int test_passed = 0;
    fs::path work_dir = "stage_model_input_archive_test";
    fs::remove_all( work_dir );
    fs::create_directories( work_dir );

    test_count++;
    {
        fs::path source_archive;
        fs::path slot_path = work_dir / "slot";
        auto result = create_md5_named_zip( work_dir, "nested_payload.txt", "nested archive content", source_archive )
                          ? stage_model_input_archive( source_archive, slot_path, fs::path( "ifsdata" ), "nested_stage" )
                          : InputStageResult{};
        if ( result.ok ) {
            fs::path unzip_dir = slot_path / "ifsdata";
            fs::path copied_archive = unzip_dir / source_archive.filename();
            fs::path extracted_file = unzip_dir / "nested_payload.txt";
            if ( fs::exists( copied_archive ) && fs::exists( extracted_file ) ) {
                test_passed++;
            } else {
                std::cerr << "  Test 1 FAILED: expected copied archive and extracted file in nested directory\n";
            }
        } else {
            std::cerr << "  Test 1 FAILED: staging nested archive failed";
            if ( !result.message.empty() ) {
                std::cerr << ": " << result.message;
            }
            std::cerr << '\n';
        }
    }

    test_count++;
    {
        fs::path source_archive;
        fs::path slot_path = work_dir / "slot_root";
        auto result = create_md5_named_zip( work_dir, "root_payload.txt", "root archive content", source_archive )
                          ? stage_model_input_archive( source_archive, slot_path, fs::path( "." ), "root_stage" )
                          : InputStageResult{};
        if ( result.ok ) {
            fs::path copied_archive = slot_path / source_archive.filename();
            fs::path extracted_file = slot_path / "root_payload.txt";
            if ( fs::exists( copied_archive ) && fs::exists( extracted_file ) ) {
                test_passed++;
            } else {
                std::cerr << "  Test 2 FAILED: expected copied archive and extracted file in slot root\n";
            }
        } else {
            std::cerr << "  Test 2 FAILED: staging root archive failed";
            if ( !result.message.empty() ) {
                std::cerr << ": " << result.message;
            }
            std::cerr << '\n';
        }
    }

    fs::remove_all( work_dir );
    std::cout << "  stage_model_input_archive: " << test_passed << "/" << test_count << " tests passed\n";
    if ( test_passed == test_count ) {
        SUCCESS;
        return EXIT_SUCCESS;
    }
    FAIL;
    return EXIT_FAILURE;
}

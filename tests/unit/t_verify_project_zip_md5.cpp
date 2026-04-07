/**
 * @file t_verify_project_zip_md5.cpp
 * @brief Unit test for verify_project_zip_md5()
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

int t_verify_project_zip_md5()
{
    TEST( "t_verify_project_zip_md5" );

    int test_count = 0;
    int test_passed = 0;
    fs::path work_dir = "verify_project_zip_md5_test";
    fs::remove_all( work_dir );
    fs::create_directories( work_dir );

    test_count++;
    {
        fs::path archive_path;
        if ( create_md5_named_zip( work_dir, "payload.txt", "hello world", archive_path ) && verify_project_zip_md5( archive_path ) ) {
            test_passed++;
        } else {
            std::cerr << "  Test 1 FAILED: expected checksum verification to succeed\n";
        }
    }

    test_count++;
    {
        fs::path archive_path;
        if ( create_md5_named_zip( work_dir, "payload_bad.txt", "checksum mismatch", archive_path ) ) {
            fs::path bad_name = work_dir / "jf_00000000000000000000000000000000";
            fs::rename( archive_path, bad_name );
            if ( !verify_project_zip_md5( bad_name ) ) {
                test_passed++;
            } else {
                std::cerr << "  Test 2 FAILED: expected checksum verification to fail for mismatched jf_ name\n";
            }
        } else {
            std::cerr << "  Test 2 FAILED: could not create test archive\n";
        }
    }

    test_count++;
    {
        fs::path archive_path;
        if ( create_md5_named_zip( work_dir, "payload_invalid.txt", "invalid name", archive_path ) ) {
            fs::path invalid_name = work_dir / "not_a_jf_file";
            fs::rename( archive_path, invalid_name );
            if ( !verify_project_zip_md5( invalid_name ) ) {
                test_passed++;
            } else {
                std::cerr << "  Test 3 FAILED: expected checksum verification to fail for invalid filename\n";
            }
        } else {
            std::cerr << "  Test 3 FAILED: could not create test archive\n";
        }
    }

    fs::remove_all( work_dir );
    std::cout << "  verify_project_zip_md5: " << test_passed << "/" << test_count << " tests passed\n";
    if ( test_passed == test_count ) {
        SUCCESS;
        return EXIT_SUCCESS;
    }
    FAIL;
    return EXIT_FAILURE;
}

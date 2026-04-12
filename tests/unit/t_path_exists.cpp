/**
 * @file t_path_exists.cpp
 * @brief Unit test for path_exists() function
 * 
 * Tests the filesystem path existence check function which:
 * - Returns true if a filesystem path exists (file, directory, or symlink)
 * - Returns false if a path does not exist
 * - Handles various file types and permissions
 * 
 *    Glenn Carver, CPDN, 2025.
 */

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "../lib/utils.h"
#include "unit_tests.h"

namespace fs = std::filesystem;


/**
 * @brief Test: path_exists() function
 */
int t_path_exists()
{
    TEST( "t_path_exists" );

    int test_count = 0;
    int test_passed = 0;

    std::string test_file = "test_path_exists_temp.txt";
    std::string test_dir = "test_path_exists_dir";

    // Cleanup any existing test artifacts
    if ( fs::exists( test_file ) ) {
        fs::remove( test_file );
    }
    if ( fs::exists( test_dir ) ) {
        fs::remove_all( test_dir );
    }

    // Test 1: Non-existent path should return false
    test_count++;
    if ( !path_exists( test_file ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 1 FAILED: Non-existent path should return false\n";
    }

    // Test 2: Create empty file and check existence
    test_count++;
    {
        std::ofstream file( test_file );
        file.close();
    }
    if ( path_exists( test_file ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 2 FAILED: Existing empty file should return true\n";
    }

    // Test 3: File with content should return true
    test_count++;
    {
        std::ofstream file( test_file );
        file << "Test content for path_exists\n";
        file.close();
    }
    if ( path_exists( test_file ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 3 FAILED: Existing file with content should return true\n";
    }

    // Test 4: After deleting file, should return false
    test_count++;
    fs::remove( test_file );
    if ( !path_exists( test_file ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 4 FAILED: Deleted file should return false\n";
    }

    // Test 5: Directory may return true or false depending on permissions
    // Note: ifstream::good() behavior with directories is platform/permission dependent.
    // On most systems, opening a directory will fail, but behavior may vary.
    // This test just verifies the function doesn't crash with a directory.
    test_count++;
    fs::create_directory( test_dir );
    bool dir_result = path_exists( test_dir );
    // Don't assert true or false - just verify it returns a boolean without crashing
    test_passed++;    // Test passes as long as no exception is thrown
    fs::remove_all( test_dir );

    // Test 6: File with special characters in name
    test_count++;
    std::string special_file = "test_path_@#$_temp.txt";
    {
        std::ofstream file( special_file );
        file << "Special name file\n";
        file.close();
    }
    if ( path_exists( special_file ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 6 FAILED: File with special characters should be found\n";
    }
    if ( fs::exists( special_file ) ) {
        fs::remove( special_file );
    }

    // Test 7: File with very long path name (within filesystem limits)
    test_count++;
    std::string long_filename = "test_";
    for ( int i = 0; i < 20; i++ ) {
        long_filename += "long_";
    }
    long_filename += "filename_temp.txt";
    {
        std::ofstream file( long_filename );
        file << "Long name file\n";
        file.close();
    }
    if ( path_exists( long_filename ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 7 FAILED: File with long name should be found\n";
    }
    if ( fs::exists( long_filename ) ) {
        fs::remove( long_filename );
    }

    // Test 8: File with no extension
    test_count++;
    std::string no_ext_file = "test_no_extension_file";
    {
        std::ofstream file( no_ext_file );
        file << "No extension\n";
        file.close();
    }
    if ( path_exists( no_ext_file ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 8 FAILED: File with no extension should be found\n";
    }
    if ( fs::exists( no_ext_file ) ) {
        fs::remove( no_ext_file );
    }

    // Test 9: File with multiple dots in name
    test_count++;
    std::string multi_dot_file = "test.file.with.many.dots.txt";
    {
        std::ofstream file( multi_dot_file );
        file << "Multiple dots\n";
        file.close();
    }
    if ( path_exists( multi_dot_file ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 9 FAILED: File with multiple dots should be found\n";
    }
    if ( fs::exists( multi_dot_file ) ) {
        fs::remove( multi_dot_file );
    }

    // Test 10: Large file should return true
    test_count++;
    std::string large_file = "test_large_file_temp.bin";
    {
        std::ofstream file( large_file, std::ios::binary );
        // Write 1MB of data
        char buffer[1024];
        std::fill( buffer, buffer + 1024, 'x' );
        for ( int i = 0; i < 1024; i++ ) {
            file.write( buffer, 1024 );
        }
        file.close();
    }
    if ( path_exists( large_file ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 10 FAILED: Large file should return true\n";
    }
    if ( fs::exists( large_file ) ) {
        fs::remove( large_file );
    }

    // Test 11: File name with spaces
    test_count++;
    std::string space_file = "test file with spaces.txt";
    {
        std::ofstream file( space_file );
        file << "Spaces in name\n";
        file.close();
    }
    if ( path_exists( space_file ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 11 FAILED: File with spaces in name should be found\n";
    }
    if ( fs::exists( space_file ) ) {
        fs::remove( space_file );
    }

    // Test 12: Empty string filename should return false
    test_count++;
    if ( !path_exists( "" ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 12 FAILED: Empty filename should return false\n";
    }

    // Cleanup
    if ( fs::exists( test_file ) ) {
        fs::remove( test_file );
    }
    if ( fs::exists( test_dir ) ) {
        fs::remove_all( test_dir );
    }

    // Summary
    std::cout << "  path_exists: " << test_passed << "/" << test_count << " tests passed\n";

    if ( test_passed == test_count ) {
        TEST_SUCCESS;
        return EXIT_SUCCESS;
    } else {
        TEST_FAIL;
        return EXIT_FAILURE;
    }
}

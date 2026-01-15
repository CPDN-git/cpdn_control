/**
 * @file t_get_tag.cpp
 * @brief Unit test for get_tag() function
 * 
 * Tests the file reference extraction function which:
 * - Extracts BOINC file references from text files (jf_* format)
 * - Detects binary ZIP files and returns empty string
 * - Handles files of various sizes
 * - Handles malformed or empty files gracefully
 * 
 *    Glenn Carver, CPDN, 2025.
 */

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "../src/cpdn_control.h"
#include "unit_tests.h"

namespace fs = std::filesystem;


/**
 * @brief Test: get_tag() function with various file formats
 */
int t_get_tag()
{
    TEST( "t_get_tag" );

    int test_count = 0;
    int test_passed = 0;
    std::string test_result;

    // Test 1: Valid file reference with delimiters
    test_count++;
    {
        std::string filename = "test_tag_1.txt";
        std::ofstream file( filename, std::ios::out | std::ios::trunc );
        file << ">../../projects/climateprediction.net/jf_ic_ancil_1234<";
        file.close();

        test_result = get_tag( filename );
        if ( test_result == "../../projects/climateprediction.net/jf_ic_ancil_1234" ) {
            test_passed++;
        } else {
            std::cerr << "  Test 1 FAILED: Expected '../../projects/climateprediction.net/jf_ic_ancil_1234', got '"
                      << test_result << "'\n";
        }
        fs::remove( filename );
    }

    // Test 2: File reference with different path structure
    test_count++;
    {
        std::string filename = "test_tag_2.txt";
        std::ofstream file( filename, std::ios::out | std::ios::trunc );
        file << ">some_path/jf_file_xyz<";
        file.close();

        test_result = get_tag( filename );
        if ( test_result == "some_path/jf_file_xyz" ) {
            test_passed++;
        } else {
            std::cerr << "  Test 2 FAILED: Expected 'some_path/jf_file_xyz', got '" << test_result << "'\n";
        }
        fs::remove( filename );
    }

    // Test 3: Binary ZIP file (should detect and return empty string)
    test_count++;
    {
        std::string filename = "test_tag_3.zip";
        std::ofstream file( filename, std::ios::out | std::ios::binary );
        // Write ZIP magic number: PK (0x504B)
        char magic[] = { 0x50, 0x4B, 0x03, 0x04 };
        file.write( magic, 4 );
        file << "This would be ZIP file content";
        file.close();

        test_result = get_tag( filename );
        if ( test_result.empty() ) {
            test_passed++;
        } else {
            std::cerr << "  Test 3 FAILED: ZIP file should return empty string, got '" << test_result << "'\n";
        }
        fs::remove( filename );
    }

    // Test 4: Empty file
    test_count++;
    {
        std::string filename = "test_tag_4.txt";
        std::ofstream file( filename, std::ios::out | std::ios::trunc );
        file.close();    // Create empty file

        test_result = get_tag( filename );
        if ( test_result.empty() ) {
            test_passed++;
        } else {
            std::cerr << "  Test 4 FAILED: Empty file should return empty string, got '" << test_result << "'\n";
        }
        fs::remove( filename );
    }

    // Test 5: File without delimiters (plain text)
    test_count++;
    {
        std::string filename = "test_tag_5.txt";
        std::ofstream file( filename, std::ios::out | std::ios::trunc );
        file << "This is just plain text without delimiters";
        file.close();

        test_result = get_tag( filename );
        if ( test_result.empty() ) {
            test_passed++;
        } else {
            std::cerr << "  Test 5 FAILED: Plain text without delimiters should return empty, got '" << test_result
                      << "'\n";
        }
        fs::remove( filename );
    }

    // Test 6: File reference at beginning of file (immediate delimiter)
    test_count++;
    {
        std::string filename = "test_tag_6.txt";
        std::ofstream file( filename, std::ios::out | std::ios::trunc );
        file << ">../projects/jf_data<";
        file.close();

        test_result = get_tag( filename );
        if ( test_result == "../projects/jf_data" ) {
            test_passed++;
        } else {
            std::cerr << "  Test 6 FAILED: Expected '../projects/jf_data', got '" << test_result << "'\n";
        }
        fs::remove( filename );
    }

    // Test 7: Reference with only opening delimiter (malformed)
    test_count++;
    {
        std::string filename = "test_tag_7.txt";
        std::ofstream file( filename, std::ios::out | std::ios::trunc );
        file << ">../projects/jf_data";    // No closing >
        file.close();

        test_result = get_tag( filename );
        // Should return empty since there's no closing delimiter
        if ( test_result.empty() ) {
            test_passed++;
        } else {
            std::cerr << "  Test 7 FAILED: Malformed reference should return empty, got '" << test_result << "'\n";
        }
        fs::remove( filename );
    }

    // Test 8: Large file reference (larger than typical but within buffer)
    test_count++;
    {
        std::string filename = "test_tag_8.txt";
        std::string longpath = ">" + std::string( 100, 'a' ) + "<";
        std::ofstream file( filename, std::ios::out | std::ios::trunc );
        file << longpath;
        file.close();

        test_result = get_tag( filename );
        if ( test_result == std::string( 100, 'a' ) ) {
            test_passed++;
        } else {
            std::cerr << "  Test 8 FAILED: Long path not extracted correctly\n";
        }
        fs::remove( filename );
    }

    // Test 9: Non-existent file
    test_count++;
    {
        std::string filename = "nonexistent_file_that_does_not_exist_12345.txt";
        test_result = get_tag( filename );
        // Should return empty string when file doesn't exist
        if ( test_result.empty() ) {
            test_passed++;
        } else {
            std::cerr << "  Test 9 FAILED: Non-existent file should return empty, got '" << test_result << "'\n";
        }
    }

    // Test 10: Multiple delimited sections (only first should be extracted)
    test_count++;
    {
        std::string filename = "test_tag_10.txt";
        std::ofstream file( filename, std::ios::out | std::ios::trunc );
        file << ">first_ref<second_data>another_ref<";
        file.close();

        test_result = get_tag( filename );
        if ( test_result == "first_ref" ) {
            test_passed++;
        } else {
            std::cerr << "  Test 10 FAILED: Expected 'first_ref', got '" << test_result << "'\n";
        }
        fs::remove( filename );
    }

    // Summary
    std::cout << "  get_tag: " << test_passed << "/" << test_count << " tests passed\n";

    if ( test_passed == test_count ) {
        SUCCESS;
        return EXIT_SUCCESS;
    } else {
        FAIL;
        return EXIT_FAILURE;
    }
}

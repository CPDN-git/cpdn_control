/**
 * @file t_fread_last_line.cpp
 * @brief Unit test for fread_last_line() function
 * 
 * Tests the stateful file reading function which:
 * - Maintains state between calls to track file position
 * - Returns only new lines added since last call
 * - Handles file truncation and recreation
 * - Gracefully handles non-existent or empty files
 * 
 * NOTE: This function uses static variables to maintain state between calls.
 * Tests are designed to work with this design pattern.
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
 * @brief Test: fread_last_line() function with stateful file reading
 * 
 * NOTE: This function maintains static state between calls. The test
 * simulates realistic usage patterns where the file is repeatedly appended to.
 */
int t_fread_last_line()
{
    TEST( "t_fread_last_line" );

    int test_count = 0;
    int test_passed = 0;
    std::string result_line;
    std::string test_file = "test_fread_last_line.txt";

    // Cleanup any existing test file to start fresh
    if ( fs::exists( test_file ) ) {
        fs::remove( test_file );
    }

    // Test 1: Non-existent file should return false and empty string
    test_count++;
    result_line = "should_be_cleared";
    if ( !fread_last_line( test_file, result_line ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 1 FAILED: Non-existent file should return false\n";
    }

    // Test 2: Create file with single line and read it (should return true)
    test_count++;
    {
        std::ofstream file( test_file, std::ios::out | std::ios::trunc );
        file << "First line of output\n";
        file.close();
    }
    result_line.clear();
    if ( fread_last_line( test_file, result_line ) && result_line == "First line of output" ) {
        test_passed++;
    } else {
        std::cerr << "  Test 2 FAILED: Expected 'First line of output', got '" << result_line << "'\n";
    }

    // Test 3: Second read of same file (no new lines)
    // When there's no new content since last read, the function should return false
    // and logline should still report the current cached last line.
    test_count++;
    result_line.clear();
    if ( !fread_last_line( test_file, result_line ) && result_line == "First line of output" ) {
        test_passed++;
    } else {
        std::cerr << "  Test 3 FAILED: Second read with no new content should return false, "
                  << "and return the cached last line, got logline='" << result_line << "'\n";
    }

    // Test 4: Append new line and read again (should get new line)
    test_count++;
    {
        std::ofstream file( test_file, std::ios::out | std::ios::app );
        file << "Second line of output\n";
        file.close();
    }
    result_line.clear();
    if ( fread_last_line( test_file, result_line ) && result_line == "Second line of output" ) {
        test_passed++;
    } else {
        std::cerr << "  Test 4 FAILED: Expected 'Second line of output', got '" << result_line << "'\n";
    }

    // Test 5: Append another new line (should get latest)
    test_count++;
    {
        std::ofstream file( test_file, std::ios::out | std::ios::app );
        file << "Third line of output\n";
        file.close();
    }
    result_line.clear();
    if ( fread_last_line( test_file, result_line ) && result_line == "Third line of output" ) {
        test_passed++;
    } else {
        std::cerr << "  Test 5 FAILED: Expected 'Third line of output', got '" << result_line << "'\n";
    }

    // Test 6: Multiple appended lines (should get the last one)
    test_count++;
    {
        std::ofstream file( test_file, std::ios::out | std::ios::app );
        file << "Line 4\n";
        file << "Line 5\n";
        file << "Line 6\n";
        file.close();
    }
    result_line.clear();
    if ( fread_last_line( test_file, result_line ) && result_line == "Line 6" ) {
        test_passed++;
    } else {
        std::cerr << "  Test 6 FAILED: Expected 'Line 6', got '" << result_line << "'\n";
    }

    // Test 7: File with no trailing newline on new last line
    // (append without newline to existing file)
    test_count++;
    {
        std::ofstream file( test_file, std::ios::out | std::ios::app );
        file << "Line 7";    // No \n at end
        file.close();
    }
    result_line.clear();
    if ( fread_last_line( test_file, result_line ) && result_line == "Line 7" ) {
        test_passed++;
    } else {
        std::cerr << "  Test 7 FAILED: Expected 'Line 7', got '" << result_line << "'\n";
    }

    // Test 8: File with blank lines (append blank then content)
    test_count++;
    {
        std::ofstream file( test_file, std::ios::out | std::ios::app );
        file << "\n";    // Blank line
        file << "Line after blank\n";
        file.close();
    }
    result_line.clear();
    if ( fread_last_line( test_file, result_line ) && result_line == "Line after blank" ) {
        test_passed++;
    } else {
        std::cerr << "  Test 8 FAILED: Expected 'Line after blank', got '" << result_line << "'\n";
    }

    // Test 9: Very long line (append)
    test_count++;
    {
        std::string long_line( 500, 'x' );
        std::ofstream file( test_file, std::ios::out | std::ios::app );
        file << long_line << "\n";
        file.close();
    }
    result_line.clear();
    if ( fread_last_line( test_file, result_line ) && result_line == std::string( 500, 'x' ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 9 FAILED: Long line not read correctly\n";
    }

    // Test 10: Append after long line (sequential read)
    test_count++;
    {
        std::ofstream file( test_file, std::ios::out | std::ios::app );
        file << "New short line\n";
        file.close();
    }
    result_line.clear();
    if ( fread_last_line( test_file, result_line ) && result_line == "New short line" ) {
        test_passed++;
    } else {
        std::cerr << "  Test 10 FAILED: Expected 'New short line', got '" << result_line << "'\n";
    }

    // Test 11: File with special characters (append)
    test_count++;
    {
        std::ofstream file( test_file, std::ios::out | std::ios::app );
        file << "Special: !@#$%^&*()_+-=\n";
        file.close();
    }
    result_line.clear();
    if ( fread_last_line( test_file, result_line ) && result_line == "Special: !@#$%^&*()_+-=" ) {
        test_passed++;
    } else {
        std::cerr << "  Test 11 FAILED: Special characters not handled correctly\n";
    }

    // Test 12: No new line added
    // When there's no new content since last read, the function should return false
    // and logline should still report the current cached last line.
    test_count++;
    result_line.clear();
    if ( !fread_last_line( test_file, result_line ) && result_line == "Special: !@#$%^&*()_+-=" ) {
        test_passed++;
    } else {
        std::cerr << "  Test 12 FAILED: No new content should return false and leave logline set to the cached last line, "
                  << "got logline='" << result_line << "'\n";
    }

    // Test 13: File truncation handling
    // Simulate a file truncation (e.g., model restart) where file size becomes smaller than last_offset.
    // The function should detect this and reset its state, allowing re-read of truncated file content.
    test_count++;
    {
        // Recreate file with specific content
        std::ofstream file( test_file, std::ios::out | std::ios::trunc );
        file << "Line before truncation\n";
        file << "Another line\n";
        file.close();
    }
    result_line.clear();
    fread_last_line( test_file, result_line );    // Read to establish offset

    // Now truncate the file and write new content (simulating model restart)
    {
        std::ofstream file( test_file, std::ios::out | std::ios::trunc );
        file << "After truncation\n";
        file.close();
    }
    result_line.clear();
    if ( fread_last_line( test_file, result_line ) && result_line == "After truncation" ) {
        test_passed++;
    } else {
        std::cerr << "  Test 13 FAILED: Should recover from truncation and return new content, got '" << result_line
                  << "'\n";
    }

    // Cleanup
    if ( fs::exists( test_file ) ) {
        fs::remove( test_file );
    }

    // Summary
    std::cout << "  fread_last_line: " << test_passed << "/" << test_count << " tests passed\n";

    if ( test_passed == test_count ) {
        SUCCESS;
        return EXIT_SUCCESS;
    } else {
        FAIL;
        return EXIT_FAILURE;
    }
}

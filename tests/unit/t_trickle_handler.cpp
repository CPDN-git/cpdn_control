/**
 * @file t_trickle_handler.cpp
 * @brief Unit test for TrickleHandler::read_trickle_data_file()
 * 
 * Tests reading and sanitizing trickle data from the 'trickle_data' file.
 * The file format is comma-separated real numbers (positive and negative).
 * 
 * The function:
 * - Returns empty string if file doesn't exist (silent)
 * - Returns empty string and warns if file is empty
 * - Removes trailing comma if present
 * - Sanitizes to keep only digits, commas, and minus signs; warns if chars stripped
 * - Warns and truncates if content exceeds 509 characters
 * 
 * This test is declared as a friend of TrickleHandler to access the private
 * read_trickle_data_file() method. See trickle_handler.h for friend declaration.
 * 
 *     Glenn Carver, CPDN, 2025.
 */

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../api/trickle_handler.h"
#include "unit_tests.h"

namespace fs = std::filesystem;

/**
 * @brief Test: TrickleHandler::read_trickle_data_file()
 * 
 * This function is declared as a friend of TrickleHandler in the header,
 * allowing it to call the private read_trickle_data_file() method.
 */
int t_trickle_handler()
{
    TEST( "Trickle Handler" );

    int test_count = 0;
    int test_passed = 0;

    // Use a test directory for isolation
    std::string test_dir = "trickle_test_temp/";
    
    // Save current working directory for restoration later
    fs::path original_cwd = fs::current_path();
    
    // Cleanup any existing test artifacts
    if ( fs::exists( test_dir ) ) {
        fs::remove_all( test_dir );
    }
    fs::create_directory( test_dir );
    
    // Change to test directory so trickle_data is found/created there
    fs::current_path( test_dir );

    // Test 1: Non-existent file returns empty string (silent)
    test_count++;
    {
        // Ensure file doesn't exist
        if ( fs::exists( "trickle_data" ) ) {
            fs::remove( "trickle_data" );
        }

        TrickleHandler handler( "test_wu", "test_result", "test_slot" );
        std::string result = handler.read_trickle_data_file();

        if ( result.empty() ) {
            test_passed++;
        } else {
            std::cerr << "  Test 1 FAILED: Non-existent file should return empty string\n";
            std::cerr << "    Got: '" << result << "'\n";
        }
    }

    // Test 2: Empty file returns empty string and warns
    test_count++;
    {
        // Create empty file
        {
            std::ofstream file( "trickle_data" );
            file.close();
        }

        TrickleHandler handler( "test_wu", "test_result", "test_slot" );
        std::string result = handler.read_trickle_data_file();

        // Note: We can't easily capture stderr in this framework, so we just verify the result
        if ( result.empty() ) {
            test_passed++;
        } else {
            std::cerr << "  Test 2 FAILED: Empty file should return empty string\n";
            std::cerr << "    Got: '" << result << "'\n";
        }

        // Cleanup
        fs::remove( "trickle_data" );
    }

    // Test 3: Valid comma-separated real numbers (positive and negative)
    test_count++;
    {
        // Create file with real number data
        {
            std::ofstream file( "trickle_data" );
            file << "1.5,-2.3,3.14159,-0.5,42.0";
            file.close();
        }

        TrickleHandler handler( "test_wu", "test_result", "test_slot" );
        std::string result = handler.read_trickle_data_file();

        // Expected: dots will be sanitized away, leaving digits, commas, and minus signs
        std::string expected = "15,-23,314159,-05,420";
        if ( result == expected ) {
            test_passed++;
        } else {
            std::cerr << "  Test 3 FAILED: Valid real numbers with sanitization\n";
            std::cerr << "    Expected: '" << expected << "'\n";
            std::cerr << "    Got: '" << result << "'\n";
        }

        // Cleanup
        fs::remove( "trickle_data" );
    }

    // Test 4: Trailing comma is removed
    test_count++;
    {
        // Create file with trailing comma
        {
            std::ofstream file( "trickle_data" );
            file << "1.2,3.4,5.6,";
            file.close();
        }

        TrickleHandler handler( "test_wu", "test_result", "test_slot" );
        std::string result = handler.read_trickle_data_file();

        // Trailing comma should be removed, then sanitized (dots stripped)
        std::string expected = "12,34,56";
        if ( result == expected ) {
            test_passed++;
        } else {
            std::cerr << "  Test 4 FAILED: Trailing comma removal\n";
            std::cerr << "    Expected: '" << expected << "'\n";
            std::cerr << "    Got: '" << result << "'\n";
        }

        // Cleanup
        fs::remove( "trickle_data" );
    }

    // Test 5: Negative numbers are preserved
    test_count++;
    {
        // Create file with mixed positive and negative real numbers
        {
            std::ofstream file( "trickle_data" );
            file << "-123.456,789.012,-0.1,999.999";
            file.close();
        }

        TrickleHandler handler( "test_wu", "test_result", "test_slot" );
        std::string result = handler.read_trickle_data_file();

        // Minus signs should be preserved, dots stripped, commas preserved
        std::string expected = "-123456,789012,-01,999999";
        if ( result == expected ) {
            test_passed++;
        } else {
            std::cerr << "  Test 5 FAILED: Negative numbers preservation\n";
            std::cerr << "    Expected: '" << expected << "'\n";
            std::cerr << "    Got: '" << result << "'\n";
        }

        // Cleanup
        fs::remove( "trickle_data" );
    }

    // Test 6: Invalid characters are stripped with warning
    test_count++;
    {
        // Create file with invalid characters mixed in
        {
            std::ofstream file( "trickle_data" );
            file << "1.2,3.4!@#,5.6;6.7";
            file.close();
        }

        TrickleHandler handler( "test_wu", "test_result", "test_slot" );
        std::string result = handler.read_trickle_data_file();

        // Invalid chars (! @ # ;) should be stripped, dots stripped, commas preserved
        // Note: 5.6;6.7 becomes 5667 (dot and semicolon removed, no comma to separate them)
        std::string expected = "12,34,5667";
        if ( result == expected ) {
            test_passed++;
        } else {
            std::cerr << "  Test 6 FAILED: Invalid character stripping\n";
            std::cerr << "    Expected: '" << expected << "'\n";
            std::cerr << "    Got: '" << result << "'\n";
        }

        // Cleanup
        fs::remove( "trickle_data" );
    }

    // Test 7: Content exceeding 509 characters is truncated
    test_count++;
    {
        // Create file with content exceeding 509 chars (after sanitization)
        {
            std::ofstream file( "trickle_data" );
            // Create a string with 520 valid characters (digits, commas, minus signs)
            std::string long_content;
            for ( int i = 0; i < 50; ++i ) {
                long_content += "1,2,3,4,5,";  // 10 chars per iteration, 50 iterations = 500 chars
            }
            long_content += "6,7,8,9,0,1,2,3,4,5";  // Add 20 more chars to exceed 509
            file << long_content;
            file.close();
        }

        TrickleHandler handler( "test_wu", "test_result", "test_slot" );
        std::string result = handler.read_trickle_data_file();

        // Should be truncated to exactly 509 characters
        if ( result.length() == 509 ) {
            test_passed++;
        } else {
            std::cerr << "  Test 7 FAILED: Content exceeding 509 chars should be truncated\n";
            std::cerr << "    Expected length: 509\n";
            std::cerr << "    Got length: " << result.length() << "\n";
        }

        // Cleanup
        fs::remove( "trickle_data" );
    }

    // Test 8: Only digits and commas (no sanitization needed)
    test_count++;
    {
        // Real number data without dots or spaces (pre-sanitized by source)
        {
            std::ofstream file( "trickle_data" );
            file << "123,456,789,-100,0,-50";
            file.close();
        }

        TrickleHandler handler( "test_wu", "test_result", "test_slot" );
        std::string result = handler.read_trickle_data_file();

        // Should pass through unchanged
        std::string expected = "123,456,789,-100,0,-50";
        if ( result == expected ) {
            test_passed++;
        } else {
            std::cerr << "  Test 8 FAILED: Pre-sanitized data should pass through\n";
            std::cerr << "    Expected: '" << expected << "'\n";
            std::cerr << "    Got: '" << result << "'\n";
        }

        // Cleanup
        fs::remove( "trickle_data" );
    }

    // Cleanup test directory and restore original working directory
    fs::current_path( original_cwd );
    if ( fs::exists( test_dir ) ) {
        fs::remove_all( test_dir );
    }

    // Summary
    if ( test_passed == test_count ) {
        SUCCESS;
        return EXIT_SUCCESS;
    } else {
        std::cerr << "  " << test_passed << "/" << test_count << " tests passed\n";
        return EXIT_FAILURE;
    }
}

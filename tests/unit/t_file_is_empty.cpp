/**
 * @file t_file_is_empty.cpp
 * @brief Unit test for file_is_empty() function
 * 
 * Tests the file size check function which:
 * - Returns true if a file is exactly 0 bytes
 * - Returns false if a file contains any data
 * - Handles various file types and sizes
 * - Throws exception if file does not exist
 * 
 *    Glenn Carver, CPDN, 2025.
 */

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "../lib/utils.h"
#include "unit_tests.h"

namespace fs = std::filesystem;


/**
 * @brief Test: file_is_empty() function
 */
int t_file_is_empty()
{
    TEST( "t_file_is_empty" );

    int test_count = 0;
    int test_passed = 0;

    std::string test_file = "test_file_is_empty_temp.txt";
    std::string test_file2 = "test_file_is_empty_temp2.txt";

    // Cleanup any existing test artifacts
    if ( fs::exists( test_file ) ) {
        fs::remove( test_file );
    }
    if ( fs::exists( test_file2 ) ) {
        fs::remove( test_file2 );
    }

    // Test 1: Create truly empty file (0 bytes) - should return true
    test_count++;
    {
        std::ofstream file( test_file );
        file.close();    // Close without writing anything
    }
    if ( file_is_empty( test_file ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 1 FAILED: Zero-byte file should return true\n";
    }

    // Test 2: File with single character - should return false
    test_count++;
    {
        std::ofstream file( test_file2 );
        file << "x";
        file.close();
    }
    if ( !file_is_empty( test_file2 ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 2 FAILED: File with 1 byte should return false\n";
    }
    fs::remove( test_file2 );

    // Test 3: File with single newline - should return false (1 byte)
    test_count++;
    {
        std::ofstream file( test_file2 );
        file << "\n";
        file.close();
    }
    if ( !file_is_empty( test_file2 ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 3 FAILED: File with newline (1 byte) should return false\n";
    }
    fs::remove( test_file2 );

    // Test 4: File with content on single line - should return false
    test_count++;
    {
        std::ofstream file( test_file2 );
        file << "This is a test line";
        file.close();
    }
    if ( !file_is_empty( test_file2 ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 4 FAILED: File with content should return false\n";
    }
    fs::remove( test_file2 );

    // Test 5: File with multiple lines - should return false
    test_count++;
    {
        std::ofstream file( test_file2 );
        file << "Line 1\n";
        file << "Line 2\n";
        file << "Line 3\n";
        file.close();
    }
    if ( !file_is_empty( test_file2 ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 5 FAILED: File with multiple lines should return false\n";
    }
    fs::remove( test_file2 );

    // Test 6: Binary file with zeros - should return false (even if zeros)
    test_count++;
    {
        std::ofstream file( test_file2, std::ios::binary );
        char zero = '\0';
        file.write( &zero, 1 );
        file.close();
    }
    if ( !file_is_empty( test_file2 ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 6 FAILED: File with null byte should return false\n";
    }
    fs::remove( test_file2 );

    // Test 7: Large file - should return false
    test_count++;
    {
        std::ofstream file( test_file2, std::ios::binary );
        char buffer[1024];
        std::fill( buffer, buffer + 1024, 'x' );
        for ( int i = 0; i < 1024; i++ ) {
            file.write( buffer, 1024 );
        }
        file.close();
    }
    if ( !file_is_empty( test_file2 ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 7 FAILED: Large file should return false\n";
    }
    fs::remove( test_file2 );

    // Test 8: File with only spaces - should return false
    test_count++;
    {
        std::ofstream file( test_file2 );
        file << "     ";    // Just spaces
        file.close();
    }
    if ( !file_is_empty( test_file2 ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 8 FAILED: File with spaces should return false\n";
    }
    fs::remove( test_file2 );

    // Test 9: File with only newlines - should return false
    test_count++;
    {
        std::ofstream file( test_file2 );
        file << "\n\n\n";    // Just newlines
        file.close();
    }
    if ( !file_is_empty( test_file2 ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 9 FAILED: File with newlines should return false\n";
    }
    fs::remove( test_file2 );

    // Test 10: Truncated file (write then truncate to 0) - should return true
    test_count++;
    {
        std::ofstream file( test_file2 );
        file << "Some content that will be truncated";
        file.close();
    }
    {
        std::ofstream file( test_file2, std::ios::out | std::ios::trunc );
        file.close();
    }
    if ( file_is_empty( test_file2 ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 10 FAILED: Truncated file should be empty\n";
    }
    fs::remove( test_file2 );

    // Test 11: File with special characters - should return false
    test_count++;
    {
        std::ofstream file( test_file2 );
        file << "Special: !@#$%^&*()\n";
        file.close();
    }
    if ( !file_is_empty( test_file2 ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 11 FAILED: File with special characters should return false\n";
    }
    fs::remove( test_file2 );

    // Test 12: Empty file created with different method - should return true
    test_count++;
    std::string empty_file = "test_empty_created_differently.txt";
    {
        std::ofstream file( empty_file, std::ios::out | std::ios::trunc );
        // Don't write anything
    }
    if ( file_is_empty( empty_file ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 12 FAILED: Empty file created with trunc should return true\n";
    }
    if ( fs::exists( empty_file ) ) {
        fs::remove( empty_file );
    }

    // Test 13: Non-existent file should throw exception
    test_count++;
    std::string nonexistent = "this_file_definitely_does_not_exist_12345.txt";
    try {
        file_is_empty( nonexistent );
        std::cerr << "  Test 13 FAILED: Non-existent file should throw exception\n";
    } catch ( const std::exception& e ) {
        // Expected behavior
        test_passed++;
    }

    // Cleanup
    if ( fs::exists( test_file ) ) {
        fs::remove( test_file );
    }
    if ( fs::exists( test_file2 ) ) {
        fs::remove( test_file2 );
    }

    // Summary
    std::cout << "  file_is_empty: " << test_passed << "/" << test_count << " tests passed\n";

    if ( test_passed == test_count ) {
        SUCCESS;
        return EXIT_SUCCESS;
    } else {
        FAIL;
        return EXIT_FAILURE;
    }
}

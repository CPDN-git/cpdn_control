/**
 * @file t_check_parse_int.cpp
 * @brief Unit test for parse_int() function
 *
 * Tests the integer parsing function which:
 * - Converts strings to integer values
 * - Reports parsing failures via error string
 * - Handles overflow/underflow and invalid input
 * - Handles edge cases like leading/trailing whitespace and zeros
 *
 *    Glenn Carver, CPDN, 2025.
 */

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

#include "../lib/utils.h"
#include "unit_tests.h"


/**
 * @brief Test: parse_int() function with various input formats
 */
int t_check_parse_int()
{
    TEST( "t_check_parse_int" );

    int test_count = 0;
    int test_passed = 0;
    int test_index = 1;

    auto expect_success = [&]( const std::string& raw, int expected ) {
        test_count++;
        std::string input = raw;
        int value = 0;
        std::string err_msg = "preset";
        if ( parse_int( input, value, err_msg ) && value == expected && err_msg.empty() ) {
            test_passed++;
        } else {
            std::cerr << "  Test " << test_index << " FAILED: '" << raw << "' should parse to " << expected << "\n";
            if ( !err_msg.empty() ) {
                std::cerr << "    err_msg: " << err_msg << "\n";
            }
        }
        test_index++;
    };

    auto expect_failure = [&]( const std::string& raw ) {
        test_count++;
        std::string input = raw;
        int value = 0;
        std::string err_msg;
        if ( !parse_int( input, value, err_msg ) && !err_msg.empty() ) {
            test_passed++;
        } else {
            std::cerr << "  Test " << test_index << " FAILED: '" << raw << "' should be invalid\n";
            if ( err_msg.empty() ) {
                std::cerr << "    err_msg: <empty>\n";
            }
        }
        test_index++;
    };

    // Success cases
    expect_success( "42", 42 );
    expect_success( "0", 0 );
    expect_success( "-123", -123 );
    expect_success( "2147483647", std::numeric_limits<int>::max() );
    expect_success( "-2147483648", std::numeric_limits<int>::min() );
    expect_success( "00123", 123 );
    expect_success( "-00042", -42 );
    expect_success( "  17  ", 17 );

    // Failure cases
    expect_failure( "123abc" );
    expect_failure( "abc" );
    expect_failure( "" );
    expect_failure( "   " );
    expect_failure( "123.45" );
    expect_failure( "12 34" );
    expect_failure( "0x1A" );
    expect_failure( "9999999999" );
    expect_failure( "-9999999999" );

    // Summary
    std::cout << "  parse_int: " << test_passed << "/" << test_count << " tests passed\n";

    if ( test_passed == test_count ) {
        TEST_SUCCESS;
        return EXIT_SUCCESS;
    } else {
        TEST_FAIL;
        return EXIT_FAILURE;
    }
}

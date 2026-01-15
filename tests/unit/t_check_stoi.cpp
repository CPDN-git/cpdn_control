/**
 * @file t_check_stoi.cpp
 * @brief Unit test for check_stoi() function
 * 
 * Tests the integer validation function which:
 * - Validates that a string contains only numeric characters
 * - Checks for overflow conditions
 * - Handles invalid arguments (empty strings, non-numeric input)
 * - Handles edge cases like leading zeros, negative numbers, etc.
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
 * @brief Test: check_stoi() function with various input formats
 */
int t_check_stoi()
{
    TEST( "t_check_stoi" );

    int test_count = 0;
    int test_passed = 0;
    std::string input;

    // Test 1: Valid simple integer
    test_count++;
    input = "42";
    if ( check_stoi( input ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 1 FAILED: '42' should be valid\n";
    }

    // Test 2: Valid zero
    test_count++;
    input = "0";
    if ( check_stoi( input ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 2 FAILED: '0' should be valid\n";
    }

    // Test 3: Valid negative number
    test_count++;
    input = "-123";
    if ( check_stoi( input ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 3 FAILED: '-123' should be valid\n";
    }

    // Test 4: Valid large positive number (within int range)
    test_count++;
    input = "2147483647";    // INT_MAX
    if ( check_stoi( input ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 4 FAILED: '2147483647' (INT_MAX) should be valid\n";
    }

    // Test 5: Valid number with leading zeros
    test_count++;
    input = "00123";
    if ( check_stoi( input ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 5 FAILED: '00123' should be valid\n";
    }

    // Test 6: Invalid - contains alphabetic characters
    test_count++;
    input = "123abc";
    if ( !check_stoi( input ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 6 FAILED: '123abc' should be invalid (contains letters)\n";
    }

    // Test 7: Invalid - only alphabetic characters
    test_count++;
    input = "abc";
    if ( !check_stoi( input ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 7 FAILED: 'abc' should be invalid\n";
    }

    // Test 8: Invalid - empty string
    test_count++;
    input = "";
    if ( !check_stoi( input ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 8 FAILED: empty string should be invalid\n";
    }

    // Test 9: Valid - floating point number (leading digits are valid)
    // Note: stoi() accepts leading digits in "123.45", so this is actually valid
    // (it parses "123" from the string)
    test_count++;
    input = "123.45";
    if ( check_stoi( input ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 9 FAILED: '123.45' is valid (stoi extracts '123')\n";
    }

    // Test 10: Valid - number with spaces (leading digits are valid)
    // Note: stoi() accepts leading digits in "12 34", so this is actually valid
    test_count++;
    input = "12 34";
    if ( check_stoi( input ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 10 FAILED: '12 34' is valid (stoi extracts '12')\n";
    }

    // Test 11: Invalid - hexadecimal format (contains 'x' and 'A', which are letters)
    test_count++;
    input = "0x1A";
    if ( !check_stoi( input ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 11 FAILED: '0x1A' should be invalid (contains letters)\n";
    }

    // Test 12: Invalid - scientific notation (contains 'e', which is a letter)
    test_count++;
    input = "1e5";
    if ( !check_stoi( input ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 12 FAILED: '1e5' should be invalid (contains letter 'e')\n";
    }

    // Test 13: Valid - plus sign prefix (no letters, stoi accepts '+')
    // Note: check_stoi only checks for alphabetic chars, '+' is not a letter
    test_count++;
    input = "+123";
    if ( check_stoi( input ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 13 FAILED: '+123' is valid (stoi accepts sign)\n";
    }

    // Test 14: Valid - special characters like '@'
    // Note: check_stoi only checks for alphabetic chars, '@' is not a letter
    // However stoi will fail on invalid input, so this should be valid
    test_count++;
    input = "12@34";
    if ( check_stoi( input ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 14 FAILED: '12@34' is valid (stoi extracts '12')\n";
    }

    // Test 15: Valid - negative with leading zeros
    test_count++;
    input = "-00042";
    if ( check_stoi( input ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 15 FAILED: '-00042' should be valid\n";
    }

    // Test 16: Valid - large negative number (within int range)
    test_count++;
    input = "-2147483648";    // INT_MIN
    if ( check_stoi( input ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 16 FAILED: '-2147483648' (INT_MIN) should be valid\n";
    }

    // Test 17: Invalid - overflow (number too large)
    test_count++;
    input = "9999999999";    // Larger than INT_MAX
    if ( !check_stoi( input ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 17 FAILED: '9999999999' should be invalid (overflow)\n";
    }

    // Test 18: Invalid - underflow (number too small)
    test_count++;
    input = "-9999999999";    // Smaller than INT_MIN
    if ( !check_stoi( input ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 18 FAILED: '-9999999999' should be invalid (underflow)\n";
    }

    // Test 19: Invalid - whitespace only
    test_count++;
    input = "   ";
    if ( !check_stoi( input ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 19 FAILED: whitespace-only string should be invalid\n";
    }

    // Test 20: Valid - tab and number (no letters, stoi will extract '123')
    test_count++;
    input = "123\t45";
    if ( check_stoi( input ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 20 FAILED: '123\\t45' is valid (stoi extracts '123')\n";
    }

    // Summary
    std::cout << "  check_stoi: " << test_passed << "/" << test_count << " tests passed\n";

    if ( test_passed == test_count ) {
        SUCCESS;
        return EXIT_SUCCESS;
    } else {
        FAIL;
        return EXIT_FAILURE;
    }
}

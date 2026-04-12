/**
 * @file t_parse_namelist_key_value.cpp
 * @brief Unit test for parse_namelist_key_value() function
 *
 * Tests Fortran namelist parsing:
 * - Lines starting with ! are ignored (comments)
 * - Trailing comma is stripped
 * - Trailing ! comment is stripped
 * - Trailing comma and comment are stripped
 *
 *    Glenn Carver, CPDN, 2025.
 */

#include <cstdlib>
#include <iostream>
#include <string>

#include "../lib/utils.h"
#include "unit_tests.h"


/**
 * @brief Test: parse_namelist_key_value() function with namelist-specific rules
 */
int t_parse_namelist_key_value()
{
    TEST( "t_parse_namelist_key_value" );

    std::string key;
    std::string value;
    int test_count = 0;
    int test_passed = 0;

    // Test 1: Namelist comment line should return false
    test_count++;
    key.clear();
    value.clear();
    if ( !parse_namelist_key_value( "  ! NSTEPS=12,", key, value ) ) {
        test_passed++;
    } else {
        std::cerr << "  Test 1 FAILED: Namelist comment line should return false\n";
    }

    // Test 2: Namelist value parsing with trailing comma
    test_count++;
    key.clear();
    value.clear();
    if ( parse_namelist_key_value( "NSTEPS=20,", key, value ) && key == "NSTEPS" && value == "20" ) {
        test_passed++;
    } else {
        std::cerr << "  Test 2 FAILED: Namelist value parsing with trailing comma\n";
    }

    // Test 3: Namelist value parsing with trailing comment
    test_count++;
    key.clear();
    value.clear();
    if ( parse_namelist_key_value( "KEY=VALUE ! this is a comment", key, value ) && key == "KEY" && value == "VALUE" ) {
        test_passed++;
    } else {
        std::cerr << "  Test 3 FAILED: Namelist value parsing with trailing comment\n";
    }

    // Test 4: Namelist value parsing with trailing comma and comment
    test_count++;
    key.clear();
    value.clear();
    if ( parse_namelist_key_value( "KEY=VALUE, ! this is a comment", key, value ) && key == "KEY" &&
         value == "VALUE" ) {
        test_passed++;
    } else {
        std::cerr << "  Test 4 FAILED: Namelist value parsing with trailing comma and comment\n";
    }

    // Summary
    std::cout << "  parse_namelist_key_value: " << test_passed << "/" << test_count << " tests passed\n";

    if ( test_passed == test_count ) {
        TEST_SUCCESS;
        return EXIT_SUCCESS;
    } else {
        TEST_FAIL;
        return EXIT_FAILURE;
    }
}

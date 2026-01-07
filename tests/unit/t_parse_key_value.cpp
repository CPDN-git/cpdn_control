/**
 * @file t_parse_key_value.cpp
 * @brief Unit test for parse_key_value() function
 * 
 * Tests the configuration parsing function which handles multiple input formats:
 * - Standard KEY=VALUE format
 * - Custom delimiter support
 * - Whitespace handling (leading, trailing, around delimiters)
 * - Quote handling for values
 * - Edge cases: empty lines, comments, malformed input
 * 
 *    Glenn Carver, CPDN, 2025.
 */

#include <string>
#include <iostream>
#include <cstdlib>

#include "../lib/utils.h"
#include "unit_tests.h"


/**
 * @brief Test: parse_key_value() function with various input formats
 */
int t_parse_key_value()
{
    TEST("t_parse_key_value");
    
    std::string key;
    std::string value;
    int test_count = 0;
    int test_passed = 0;

    // Test 1: Standard KEY=VALUE format with explicit delimiter
    test_count++;
    key.clear(); value.clear();
    if (parse_key_value("OMP_NUM_THREADS=4", key, value, '=') &&
        key == "OMP_NUM_THREADS" && value == "4") {
        test_passed++;
    } else {
        std::cerr << "  Test 1 FAILED: Standard KEY=VALUE with explicit delimiter\n";
    }

    // Test 2: Custom delimiter support
    test_count++;
    key.clear(); value.clear();
    if (parse_key_value("MODEL:fast", key, value, ':') &&
        key == "MODEL" && value == "fast") {
        test_passed++;
    } else {
        std::cerr << "  Test 2 FAILED: Custom delimiter\n";
    }

    // Test 3: Whitespace around equals sign
    test_count++;
    key.clear(); value.clear();
    if (parse_key_value("  MY_VAR = somevalue  ", key, value) &&
        key == "MY_VAR" && value == "somevalue") {
        test_passed++;
    } else {
        std::cerr << "  Test 3 FAILED: Whitespace around equals sign\n";
    }

    // Test 4: Leading whitespace before key
    test_count++;
    key.clear(); value.clear();
    if (parse_key_value("   VAR=value123", key, value) &&
        key == "VAR" && value == "value123") {
        test_passed++;
    } else {
        std::cerr << "  Test 4 FAILED: Leading whitespace\n";
    }

    // Test 5: Unquoted value with internal spaces (spaces preserved)
    test_count++;
    key.clear(); value.clear();
    if (parse_key_value("MY_PATH=/some/path/ to/file", key, value) &&
        key == "MY_PATH" && value == "/some/path/ to/file") {
        test_passed++;
    } else {
        std::cerr << "  Test 5 FAILED: Path value without quotes\n";
    }

    // Test 6: Comment line should return false
    test_count++;
    key.clear(); value.clear();
    if (!parse_key_value("# This is a comment", key, value)) {
        test_passed++;
    } else {
        std::cerr << "  Test 6 FAILED: Comment line should return false\n";
    }

    // Test 7: Empty line should return false
    test_count++;
    key.clear(); value.clear();
    if (!parse_key_value("", key, value)) {
        test_passed++;
    } else {
        std::cerr << "  Test 7 FAILED: Empty line should return false\n";
    }

    // Test 8: Whitespace-only line should return false
    test_count++;
    key.clear(); value.clear();
    if (!parse_key_value("   \t  ", key, value)) {
        test_passed++;
    } else {
        std::cerr << "  Test 8 FAILED: Whitespace-only line should return false\n";
    }

    // Test 9: No equals sign should return false
    test_count++;
    key.clear(); value.clear();
    if (!parse_key_value("INVALID_FORMAT", key, value)) {
        test_passed++;
    } else {
        std::cerr << "  Test 9 FAILED: No equals sign should return false\n";
    }

    // Test 10: Empty key (equals at start) should return false
    test_count++;
    key.clear(); value.clear();
    if (!parse_key_value("=value", key, value)) {
        test_passed++;
    } else {
        std::cerr << "  Test 10 FAILED: Empty key should return false\n";
    }

    // Test 11: Quoted value with spaces inside quotes (single quotes)
    test_count++;
    key.clear(); value.clear();
    if (parse_key_value("VAR='value with spaces'", key, value) &&
        key == "VAR" && value == "value with spaces") {
        test_passed++;
    } else {
        std::cerr << "  Test 11 FAILED: Single-quoted value\n";
    }

    // Test 12: Quoted value with spaces inside quotes (double quotes)
    test_count++;
    key.clear(); value.clear();
    if (parse_key_value("VAR=\"value with spaces\"", key, value) &&
        key == "VAR" && value == "value with spaces") {
        test_passed++;
    } else {
        std::cerr << "  Test 12 FAILED: Double-quoted value\n";
    }

    // Test 13: Mixed quotes should not be treated as quote delimiters
    test_count++;
    key.clear(); value.clear();
    if (parse_key_value("VAR='value\"mixed", key, value) &&
        key == "VAR" && value == "'value\"mixed") {
        test_passed++;
    } else {
        std::cerr << "  Test 13 FAILED: Mixed quotes handling\n";
    }

    // Test 14: Empty value (just KEY=)
    test_count++;
    key.clear(); value.clear();
    if (parse_key_value("EMPTY_VAR=", key, value) &&
        key == "EMPTY_VAR" && value == "") {
        test_passed++;
    } else {
        std::cerr << "  Test 14 FAILED: Empty value\n";
    }

    // Summary
    std::cout << "  parse_key_value: " << test_passed << "/" << test_count << " tests passed\n";

    if (test_passed == test_count) {
        SUCCESS;
        return EXIT_SUCCESS;
    } else {
        FAIL;
        return EXIT_FAILURE;
    }
}

/**
 * @file t_zip_and_delete.cpp
 * @brief Unit test for zip_and_delete() function
 * 
 * Tests the zip file creation and cleanup function which:
 * - Zips a list of files into a single archive
 * - Times the compression operation
 * - Deletes the source files if zipping succeeds
 * - Does NOT delete files if zipping fails
 * - Handles file deletion errors gracefully
 * - Returns 0 on success, 1 on failure
 * 
 *    Glenn Carver, CPDN, 2025.
 */

#include <string>
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <vector>

#include "../src/cpdn_control.h"
#include "../zip/cpdn_zip.h"
#include "unit_tests.h"

namespace fs = std::filesystem;


/**
 * @brief Test: zip_and_delete() function
 */
int t_zip_and_delete()
{
    TEST("t_zip_and_delete");
    
    int test_count = 0;
    int test_passed = 0;
    
    std::string test_dir = "zip_test_temp/";
    std::string test_file1 = test_dir + "test_file1.txt";
    std::string test_file2 = test_dir + "test_file2.txt";
    std::string test_file3 = test_dir + "test_file3.txt";
    std::string zip_output = test_dir + "test_archive.zip";

    // Create test directory
    if (!fs::exists(test_dir)) {
        fs::create_directory(test_dir);
    }

    // Cleanup any existing test artifacts
    if (fs::exists(test_dir)) {
        fs::remove_all(test_dir);
    }
    fs::create_directory(test_dir);

    // Test 1: Zip and delete single file successfully
    test_count++;
    {
        // Create test file
        {
            std::ofstream file(test_file1);
            file << "This is test file 1 with some content for zipping";
            file.close();
        }
        
        std::vector<fs::path> files_to_zip;
        files_to_zip.push_back(test_file1);
        
        int result = zip_and_delete(zip_output, files_to_zip);
        
        // Check: zip file created
        bool zip_exists = fs::exists(zip_output);
        // Check: source file deleted
        bool source_deleted = !fs::exists(test_file1);
        // Check: return value is 0 (success)
        bool success = (result == 0);
        
        if (zip_exists && source_deleted && success) {
            test_passed++;
        } else {
            std::cerr << "  Test 1 FAILED: Single file zip and delete\n";
            if (!zip_exists) std::cerr << "    - Zip file not created\n";
            if (!source_deleted) std::cerr << "    - Source file not deleted\n";
            if (!success) std::cerr << "    - Return value was not 0\n";
        }
        
        // Cleanup
        if (fs::exists(zip_output)) fs::remove(zip_output);
    }

    // Test 2: Zip and delete multiple files successfully
    test_count++;
    {
        // Create test files
        {
            std::ofstream file1(test_file1);
            file1 << "Content of file 1";
            file1.close();
            
            std::ofstream file2(test_file2);
            file2 << "Content of file 2 with more text";
            file2.close();
            
            std::ofstream file3(test_file3);
            file3 << "File 3 content";
            file3.close();
        }
        
        std::vector<fs::path> files_to_zip;
        files_to_zip.push_back(test_file1);
        files_to_zip.push_back(test_file2);
        files_to_zip.push_back(test_file3);
        
        int result = zip_and_delete(zip_output, files_to_zip);
        
        // Check: zip file created
        bool zip_exists = fs::exists(zip_output);
        // Check: all source files deleted
        bool all_deleted = (!fs::exists(test_file1) && 
                           !fs::exists(test_file2) && 
                           !fs::exists(test_file3));
        // Check: return value is 0 (success)
        bool success = (result == 0);
        
        if (zip_exists && all_deleted && success) {
            test_passed++;
        } else {
            std::cerr << "  Test 2 FAILED: Multiple file zip and delete\n";
            if (!zip_exists) std::cerr << "    - Zip file not created\n";
            if (!all_deleted) {
                if (fs::exists(test_file1)) std::cerr << "    - File 1 not deleted\n";
                if (fs::exists(test_file2)) std::cerr << "    - File 2 not deleted\n";
                if (fs::exists(test_file3)) std::cerr << "    - File 3 not deleted\n";
            }
            if (!success) std::cerr << "    - Return value was not 0\n";
        }
        
        // Cleanup
        if (fs::exists(zip_output)) fs::remove(zip_output);
    }

    // Test 3: Empty file list - should zip nothing and return success
    test_count++;
    {
        std::vector<fs::path> empty_list;
        
        int result = zip_and_delete(zip_output, empty_list);
        
        // zip_and_delete should handle empty list gracefully
        // Return value should indicate success (0)
        // Note: cpdn_zip may create empty zip or fail; we test behavior
        bool success = (result == 0);
        
        if (success) {
            test_passed++;
        } else {
            std::cerr << "  Test 3 FAILED: Empty file list should return success\n";
        }
        
        // Cleanup
        if (fs::exists(zip_output)) fs::remove(zip_output);
    }

    // Test 4: Zip file with large content - test timing doesn't crash
    test_count++;
    {
        // Create a file with more substantial content
        std::string large_file = test_dir + "large_test.txt";
        {
            std::ofstream file(large_file);
            for (int i = 0; i < 10000; i++) {
                file << "Line " << i << ": This is a test line with some repeating content\n";
            }
            file.close();
        }
        
        std::vector<fs::path> files_to_zip;
        files_to_zip.push_back(large_file);
        
        int result = zip_and_delete(zip_output, files_to_zip);
        
        bool zip_exists = fs::exists(zip_output);
        bool source_deleted = !fs::exists(large_file);
        bool success = (result == 0);
        
        if (zip_exists && source_deleted && success) {
            test_passed++;
        } else {
            std::cerr << "  Test 4 FAILED: Large file zip and delete\n";
            if (!zip_exists) std::cerr << "    - Zip file not created\n";
            if (!source_deleted) std::cerr << "    - Source file not deleted\n";
            if (!success) std::cerr << "    - Return value was not 0\n";
        }
        
        // Cleanup
        if (fs::exists(zip_output)) fs::remove(zip_output);
    }

    // Test 5: Files with special characters in names
    test_count++;
    {
        std::string special_file = test_dir + "test_file_with-dash_and_underscore.txt";
        {
            std::ofstream file(special_file);
            file << "File with special characters in name";
            file.close();
        }
        
        std::vector<fs::path> files_to_zip;
        files_to_zip.push_back(special_file);
        
        int result = zip_and_delete(zip_output, files_to_zip);
        
        bool zip_exists = fs::exists(zip_output);
        bool source_deleted = !fs::exists(special_file);
        bool success = (result == 0);
        
        if (zip_exists && source_deleted && success) {
            test_passed++;
        } else {
            std::cerr << "  Test 5 FAILED: File with special characters\n";
            if (!zip_exists) std::cerr << "    - Zip file not created\n";
            if (!source_deleted) std::cerr << "    - Source file not deleted\n";
            if (!success) std::cerr << "    - Return value was not 0\n";
        }
        
        // Cleanup
        if (fs::exists(zip_output)) fs::remove(zip_output);
    }

    // Test 6: Source file doesn't exist - cpdn_zip should fail gracefully
    test_count++;
    {
        std::string nonexistent = test_dir + "nonexistent_file.txt";
        std::vector<fs::path> files_to_zip;
        files_to_zip.push_back(nonexistent);
        
        int result = zip_and_delete(zip_output, files_to_zip);
        
        // When source file doesn't exist, cpdn_zip fails and returns 1
        // zip_and_delete should return 1 (failure)
        bool returned_failure = (result == 1);
        // Zip file should NOT exist because cpdn_zip failed
        bool zip_not_created = !fs::exists(zip_output);
        
        if (returned_failure && zip_not_created) {
            test_passed++;
        } else {
            std::cerr << "  Test 6 FAILED: Nonexistent file handling\n";
            if (!returned_failure) std::cerr << "    - Should return 1 on failure, got " << result << "\n";
            if (!zip_not_created) std::cerr << "    - Zip file should not be created when source missing\n";
        }
        
        // Cleanup
        if (fs::exists(zip_output)) fs::remove(zip_output);
    }

    // Test 7: Mixed existing and non-existing files
    test_count++;
    {
        std::string existing_file = test_dir + "existing.txt";
        std::string nonexistent = test_dir + "nonexistent.txt";
        
        {
            std::ofstream file(existing_file);
            file << "This file exists";
            file.close();
        }
        
        std::vector<fs::path> files_to_zip;
        files_to_zip.push_back(existing_file);
        files_to_zip.push_back(nonexistent);
        
        int result = zip_and_delete(zip_output, files_to_zip);
        
        // cpdn_zip should fail because one file doesn't exist
        // Return value should be 1 (failure)
        // Existing file should NOT be deleted because zip_and_delete returns error
        bool returned_failure = (result == 1);
        bool existing_still_there = fs::exists(existing_file);
        
        if (returned_failure && existing_still_there) {
            test_passed++;
        } else {
            std::cerr << "  Test 7 FAILED: Mixed existing/nonexistent files\n";
            if (!returned_failure) std::cerr << "    - Should return 1 on failure\n";
            if (!existing_still_there) std::cerr << "    - Existing file should NOT be deleted on zip failure\n";
        }
        
        // Cleanup
        if (fs::exists(existing_file)) fs::remove(existing_file);
        if (fs::exists(zip_output)) fs::remove(zip_output);
    }

    // Test 8: Multiple files where some are empty
    test_count++;
    {
        std::string empty_file = test_dir + "empty.txt";
        std::string full_file = test_dir + "full.txt";
        
        // Create empty file
        {
            std::ofstream file(empty_file);
            file.close();
        }
        // Create file with content
        {
            std::ofstream file(full_file);
            file << "This file has content";
            file.close();
        }
        
        std::vector<fs::path> files_to_zip;
        files_to_zip.push_back(empty_file);
        files_to_zip.push_back(full_file);
        
        int result = zip_and_delete(zip_output, files_to_zip);
        
        bool zip_exists = fs::exists(zip_output);
        bool both_deleted = (!fs::exists(empty_file) && !fs::exists(full_file));
        bool success = (result == 0);
        
        if (zip_exists && both_deleted && success) {
            test_passed++;
        } else {
            std::cerr << "  Test 8 FAILED: Mixed empty and full files\n";
            if (!zip_exists) std::cerr << "    - Zip file not created\n";
            if (!both_deleted) std::cerr << "    - Files not deleted\n";
            if (!success) std::cerr << "    - Return value was not 0\n";
        }
        
        // Cleanup
        if (fs::exists(zip_output)) fs::remove(zip_output);
    }

    // Test 9: Repeat zip to same output file (overwrite)
    test_count++;
    {
        // Create initial file
        {
            std::ofstream file(test_file1);
            file << "Initial content";
            file.close();
        }
        
        std::vector<fs::path> files_to_zip;
        files_to_zip.push_back(test_file1);
        
        // First zip
        int result1 = zip_and_delete(zip_output, files_to_zip);
        
        // Create new file with same name
        {
            std::ofstream file(test_file1);
            file << "Updated content";
            file.close();
        }
        
        // Second zip (should overwrite)
        int result2 = zip_and_delete(zip_output, files_to_zip);
        
        bool first_success = (result1 == 0);
        bool second_success = (result2 == 0);
        bool final_source_deleted = !fs::exists(test_file1);
        
        if (first_success && second_success && final_source_deleted) {
            test_passed++;
        } else {
            std::cerr << "  Test 9 FAILED: Overwriting existing zip file\n";
            if (!first_success) std::cerr << "    - First zip failed\n";
            if (!second_success) std::cerr << "    - Second zip failed\n";
            if (!final_source_deleted) std::cerr << "    - Final source file not deleted\n";
        }
        
        // Cleanup
        if (fs::exists(zip_output)) fs::remove(zip_output);
    }

    // Final cleanup
    if (fs::exists(test_dir)) {
        fs::remove_all(test_dir);
    }

    // Report results
    std::cout << "zip_and_delete: " << test_passed << "/" << test_count << " tests passed\n";
    
    if (test_passed == test_count) {
        SUCCESS;
        return 0;
    } else {
        FAIL;
        return 1;
    }
}

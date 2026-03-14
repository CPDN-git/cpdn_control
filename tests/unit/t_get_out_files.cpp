/**
 * @file t_get_out_files.cpp
 * @brief Unit test for get_out_files() function
 *
 * Ensures files with a given suffix are discovered in the current directory.
 */

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../lib/utils.h"
#include "unit_tests.h"

namespace fs = std::filesystem;

int t_get_out_files()
{
    TEST( "t_get_out_files" );

    const fs::path original_cwd = fs::current_path();
    fs::path tmp_dir = fs::temp_directory_path() / "cpdn_get_out_files_test";

    std::error_code ec;
    fs::remove_all( tmp_dir, ec );
    fs::create_directories( tmp_dir );

    // Prepare files
    fs::current_path( tmp_dir );
    fs::path txt1 = tmp_dir / "a.txt";
    fs::path txt2 = tmp_dir / "b.txt";
    fs::path log1 = tmp_dir / "c.log";
    fs::path nested_dir = tmp_dir / "nested";
    fs::create_directories( nested_dir );

    std::ofstream( txt1.string() ).put( 'x' );
    std::ofstream( txt2.string() ).put( 'y' );
    std::ofstream( log1.string() ).put( 'z' );
    std::ofstream( ( nested_dir / "inside.txt" ).string() ).put( 'i' );    // should not be picked up (non-recursive)

    int tests = 0;
    int passed = 0;

    // Expect only top-level .txt files
    tests++;
    auto results = get_out_files( ".txt" );
    std::sort( results.begin(), results.end() );
    std::vector<std::string> expected = { txt1.filename().string(), txt2.filename().string() };
    if ( results == expected ) {
        passed++;
    } else {
        std::cerr << "  Expected {a.txt,b.txt}, got {";
        for ( size_t i = 0; i < results.size(); ++i ) {
            std::cerr << results[i];
            if ( i + 1 != results.size() )
                std::cerr << ",";
        }
        std::cerr << "}\n";
    }

    // Cleanup and restore cwd
    fs::current_path( original_cwd );
    fs::remove_all( tmp_dir, ec );

    std::cout << "  get_out_files: " << passed << "/" << tests << " tests passed\n";
    if ( passed == tests ) {
        SUCCESS;
        return EXIT_SUCCESS;
    } else {
        FAIL;
        return EXIT_FAILURE;
    }
}

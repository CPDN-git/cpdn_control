#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "../src/cpdn_control.h"
#include "unit_tests.h"

namespace fs = std::filesystem;

int t_upload_placeholders()
{
    TEST( "t_upload_placeholders" );

    int test_count = 0;
    int test_passed = 0;

    test_count++;
    if ( expected_upload_file_count( 40, 12 ) == 4 && expected_upload_file_count( 48, 12 ) == 4 && expected_upload_file_count( 5, 12 ) == 1 &&
         expected_upload_file_count( 0, 12 ) == 0 && expected_upload_file_count( 40, 0 ) == 0 ) {
        test_passed++;
    } else {
        std::cerr << "  Expected upload-file count calculation returned an unexpected value\n";
    }

    test_count++;
    {
        const fs::path temp_dir = fs::path( "upload_placeholder_test" ) / "nested";
        if ( fs::exists( temp_dir.parent_path() ) ) {
            fs::remove_all( temp_dir.parent_path() );
        }

        fs::path placeholder_path;
        std::string err_msg;
        const bool created = create_upload_placeholder_file( temp_dir, 3, 55, 100, "task ended before scheduled upload", placeholder_path, &err_msg );
        const bool exists = created && fs::exists( placeholder_path );

        bool content_ok = false;
        if ( exists ) {
            std::ifstream input( placeholder_path );
            std::string content( ( std::istreambuf_iterator<char>( input ) ), std::istreambuf_iterator<char>() );
            content_ok = content.find( "upload_file_number=3" ) != std::string::npos && content.find( "current_step=55" ) != std::string::npos &&
                         content.find( "total_steps=100" ) != std::string::npos &&
                         content.find( "reason=task ended before scheduled upload" ) != std::string::npos;
        }

        if ( exists && content_ok ) {
            test_passed++;
        } else {
            std::cerr << "  Placeholder file creation failed";
            if ( !err_msg.empty() ) {
                std::cerr << ": " << err_msg;
            }
            std::cerr << '\n';
        }

        if ( fs::exists( temp_dir.parent_path() ) ) {
            fs::remove_all( temp_dir.parent_path() );
        }
    }

    if ( test_passed == test_count ) {
        TEST_SUCCESS;
        return EXIT_SUCCESS;
    }

    TEST_FAIL;
    return EXIT_FAILURE;
}

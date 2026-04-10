// Test to check reading progress file
//
//  Glenn Carver, CPDN, 2025


#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../api/progressfile_handler.h"
#include "unit_tests.h"

namespace fs = std::filesystem;

static bool write_text_file( const fs::path& path, const std::string& content )
{
    std::ofstream out( path, std::ios::out | std::ios::trunc );
    if ( !out.is_open() ) {
        return false;
    }
    out << content;
    return static_cast<bool>( out );
}

/**
  * @brief  Test: read progress file
  */

int t_read_progress_file()
{
    TEST( "t_read_progress_file" );

    // Test setup - taken from a real batch progress file
    TaskState task;
    task.prior_acc_cpu_time = 76828.0;
    task.current_cpu_time = 76828.5;
    task.upload_file_number = 3;
    task.last_completed_step = 1055;
    task.last_upload_time = 1036800.0;
    task.model_completed = 0;

    // Generate test progress file content
    ProgressFileHandler progress_file( "." );
    std::string err_msg;
    std::cout << "Subtest: write progress file\n";
    if ( !progress_file.write( task, err_msg ) ) {
        FAIL;
        std::cout << err_msg << "\n";
        return EXIT_FAILURE;
    }

    // Check the basic format of the progress file
    std::cout << "Subtest: format check (&CPDN ... / with 6 lines between)\n";
    {
        std::ifstream in( progress_file.path() );
        if ( !in.is_open() ) {
            FAIL;
            std::cout << "Unable to open progress file: " << progress_file.path() << "\n";
            return EXIT_FAILURE;
        }

        std::vector<std::string> lines;
        std::string line;
        while ( std::getline( in, line ) ) {
            lines.push_back( line );
        }

        int cpnd_index = -1;
        int slash_index = -1;
        for ( size_t i = 0; i < lines.size(); ++i ) {
            if ( cpnd_index < 0 && lines[i].find( "&CPDN" ) != std::string::npos ) {
                cpnd_index = static_cast<int>( i );
            } else if ( cpnd_index >= 0 && lines[i] == "/" ) {
                slash_index = static_cast<int>( i );
                break;
            }
        }

        if ( cpnd_index < 0 || slash_index < 0 || slash_index <= cpnd_index ) {
            FAIL;
            std::cout << "Progress file missing &CPDN or terminating '/' line\n";
            return EXIT_FAILURE;
        }

        int count_between = slash_index - cpnd_index - 1;
        if ( count_between != 6 ) {
            FAIL;
            std::cout << "Progress file has " << count_between << " lines between &CPDN and '/', expected 6\n";
            return EXIT_FAILURE;
        }
    }

    // Check reading the progress file
    std::cout << "Subtest: read valid progress file\n";
    TaskState taskin;
    if ( !progress_file.read( taskin, err_msg ) ) {
        FAIL;
        std::cout << "Failed to read progress file: " << err_msg << "\n";
        return EXIT_FAILURE;
    }
    if ( taskin.last_completed_step != 1055 || taskin.prior_acc_cpu_time != 76828.5 || taskin.upload_file_number != 3 ||
         taskin.last_upload_time != 1036800.0 || taskin.model_completed != 0 ) {
        FAIL;
        std::cout << "last_completed_step = " << taskin.last_completed_step << ", prior_acc_cpu_time = " << taskin.prior_acc_cpu_time
                  << ", upload_number = " << taskin.upload_file_number << ", last_upload_time = " << taskin.last_upload_time
                  << ", completed = " << taskin.model_completed << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "Subtest: read legacy progress file keys\n";
    const std::string legacy_content = "! CPDN controller progress file & fortran namelist\n"
                                       "&CPDN\n"
                                       "control_pid=123\n"
                                       "prior_acc_cpu_time=76828.5\n"
                                       "upload_file_number=3\n"
                                       "last_step=1055\n"
                                       "last_upload=1036800\n"
                                       "model_completed=0\n"
                                       "/\n";
    if ( !write_text_file( progress_file.path(), legacy_content ) ) {
        FAIL;
        std::cout << "Unable to write legacy progress file\n";
        return EXIT_FAILURE;
    }
    if ( !progress_file.read( taskin, err_msg ) ) {
        FAIL;
        std::cout << "Failed to read legacy progress file: " << err_msg << "\n";
        return EXIT_FAILURE;
    }
    if ( taskin.last_completed_step != 1055 || taskin.prior_acc_cpu_time != 76828.5 || taskin.upload_file_number != 3 ||
         taskin.last_upload_time != 1036800.0 || taskin.model_completed != 0 ) {
        FAIL;
        std::cout << "last_completed_step = " << taskin.last_completed_step << ", prior_acc_cpu_time = " << taskin.prior_acc_cpu_time
                  << ", upload_number = " << taskin.upload_file_number << ", last_upload_time = " << taskin.last_upload_time
                  << ", completed = " << taskin.model_completed << "\n";
        return EXIT_FAILURE;
    }
    // Additional read failure scenarios
    std::cout << "Subtest: setup temp directory for negative tests\n";
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path tmp_dir = "tmp_progressfile_tests_" + std::to_string( now );
    std::error_code ec;
    fs::create_directory( tmp_dir, ec );
    if ( ec ) {
        FAIL;
        std::cout << "Unable to create temp dir: " << tmp_dir.string() << "\n";
        return EXIT_FAILURE;
    }

    ProgressFileHandler tmp_handler( tmp_dir.string() );

    // Missing file
    std::cout << "Subtest: read missing file\n";
    if ( tmp_handler.read( taskin, err_msg ) ) {
        FAIL;
        std::cout << "Read succeeded unexpectedly for missing file\n";
        return EXIT_FAILURE;
    }

    // Empty file
    std::cout << "Subtest: read empty file\n";
    {
        std::ofstream out( tmp_handler.path(), std::ios::out | std::ios::trunc );
    }
    if ( tmp_handler.read( taskin, err_msg ) ) {
        FAIL;
        std::cout << "Read succeeded unexpectedly for empty file\n";
        return EXIT_FAILURE;
    }

    // Invalid data
    std::cout << "Subtest: read invalid data\n";
    const std::string invalid_content = "! CPDN controller progress file & fortran namelist\n"
                                        "&CPDN\n"
                                        "control_pid=123\n"
                                        "prior_acc_cpu_time=foo\n"
                                        "upload_file_number=3\n"
                                        "last_step=1055\n"
                                        "last_upload=1036800\n"
                                        "model_completed=0\n"
                                        "/\n";
    if ( !write_text_file( tmp_handler.path(), invalid_content ) ) {
        FAIL;
        std::cout << "Unable to write invalid progress file\n";
        return EXIT_FAILURE;
    }
    if ( tmp_handler.read( taskin, err_msg ) ) {
        FAIL;
        std::cout << "Read succeeded unexpectedly for invalid data\n";
        return EXIT_FAILURE;
    }

    // Truncated file (missing required fields)
    std::cout << "Subtest: read truncated file\n";
    const std::string truncated_content = "! CPDN controller progress file & fortran namelist\n"
                                          "&CPDN\n"
                                          "control_pid=123\n"
                                          "prior_acc_cpu_time=1.0\n"
                                          "/\n";
    if ( !write_text_file( tmp_handler.path(), truncated_content ) ) {
        FAIL;
        std::cout << "Unable to write truncated progress file\n";
        return EXIT_FAILURE;
    }
    if ( tmp_handler.read( taskin, err_msg ) ) {
        FAIL;
        std::cout << "Read succeeded unexpectedly for truncated file\n";
        return EXIT_FAILURE;
    }

    std::cout << "Subtest: cleanup temp files\n";
    fs::remove( progress_file.path(), ec );
    fs::remove_all( tmp_dir, ec );

    SUCCESS;
    return EXIT_SUCCESS;
}

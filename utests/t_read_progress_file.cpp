// Test to check reading progress file
//
//  Glenn Carver, CPDN, 2025


#include <fstream>

#include "../src/cpdn_control.h"
#include "unit_tests.h"


 /**
  * @brief  Test: read progress file
  */

int t_read_progress_file()
{
    TEST("t_read_progress_file");

    // Generate test progress file content, taken from real batch
    std::string progress_filename = "progress_file_12362644";
    std::ofstream progress_test(progress_filename, std::ios::out | std::ios::trunc );
    progress_test <<
                "last_cpu_time=76828\n" <<
                "upload_file_number=3\n" <<
                "last_iter=1055\n" <<
                "last_upload=1036800\n" <<
                "model_completed=0\n";
    progress_test.close();

    // Test setup
    std::string last_iter;
    int last_cpu_time = -1;
    int upload_number = -1;
    int last_upload = -1;
    int completed = -1;

    read_progress_file(progress_filename, last_cpu_time, upload_number, last_iter, last_upload, completed );
    if ( last_iter.empty() || last_cpu_time != 76828 || upload_number != 3 || last_upload != 1036800 || completed != 0 )
    {
        FAIL;
        std::cout << "last_iter = " << last_iter << ", last_cpu_time = " << last_cpu_time
                  << ", upload_number = " << upload_number << ", last_upload = " << last_upload
                  << ", completed = " << completed << "\n";
        return EXIT_FAILURE;
    }
    SUCCESS;
    return EXIT_SUCCESS;
}
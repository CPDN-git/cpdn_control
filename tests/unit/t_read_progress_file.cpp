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
    TEST( "t_read_progress_file" );

    // Generate test progress file content, taken from real batch
    std::string progress_filename = "progress_file_12362644";
    std::ofstream progress_test( progress_filename, std::ios::out | std::ios::trunc );
    progress_test << "last_cpu_time=76828\n"
                  << "upload_file_number=3\n"
                  << "last_step=1055\n"
                  << "last_upload=1036800\n"
                  << "model_completed=0\n";
    progress_test.close();

    // Test setup - Create TaskState struct
    TaskState task;

    read_progress_file( progress_filename, task );
    if ( task.last_step.empty() || task.last_cpu_time != 76828 || task.upload_file_number != 3 || task.last_upload != 1036800 ||
         task.model_completed != 0 ) {
        FAIL;
        std::cout << "last_step = " << task.last_step << ", last_cpu_time = " << task.last_cpu_time << ", upload_number = " << task.upload_file_number
                  << ", last_upload = " << task.last_upload << ", completed = " << task.model_completed << "\n";
        return EXIT_FAILURE;
    }
    SUCCESS;
    return EXIT_SUCCESS;
}
// Test to check reading progress file
//
//  Glenn Carver, CPDN, 2025


#include <fstream>

#include "../api/progressfile_handler.h"
#include "unit_tests.h"


/**
  * @brief  Test: read progress file
  */

int t_read_progress_file()
{
    TEST( "t_read_progress_file" );

    // Test setup - taken from a real batch progress file
    TaskState task;
    task.last_cpu_time = 76828;
    task.upload_file_number = 3;
    task.last_step = "1055";
    task.last_upload = 1036800;
    task.model_completed = 0;

    // Generate test progress file content
    ProgressFileHandler progress_file( "." );
    progress_file.write( task );

    // Check reading the progress file
    TaskState taskin;
    progress_file.read( taskin );
    if ( taskin.last_step.empty() || taskin.last_cpu_time != 76828 || taskin.upload_file_number != 3 || taskin.last_upload != 1036800 ||
         taskin.model_completed != 0 ) {
        FAIL;
        std::cout << "last_step = " << taskin.last_step << ", last_cpu_time = " << taskin.last_cpu_time
                  << ", upload_number = " << taskin.upload_file_number << ", last_upload = " << taskin.last_upload
                  << ", completed = " << taskin.model_completed << "\n";
        return EXIT_FAILURE;
    }
    SUCCESS;
    return EXIT_SUCCESS;
}
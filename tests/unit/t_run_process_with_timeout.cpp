/**
 * @file t_run_process_with_timeout.cpp
 * @brief Unit test for run_process_with_timeout().
 */

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "../lib/utils.h"
#include "unit_tests.h"

namespace fs = std::filesystem;

#ifndef TIMED_PROCESS_HELPER
#error "TIMED_PROCESS_HELPER is not defined"
#endif

namespace {
void set_test_env( const char* name, const std::string& value ) { setenv( name, value.c_str(), 1 ); }

void clear_test_env( const char* name ) { unsetenv( name ); }
}    // namespace

int t_run_process_with_timeout()
{
    TEST( "t_run_process_with_timeout" );

    int test_count = 0;
    int test_passed = 0;

    fs::path tmp_dir = fs::temp_directory_path() / "cpdn_run_process_with_timeout_test";
    std::error_code ec;
    fs::remove_all( tmp_dir, ec );
    fs::create_directories( tmp_dir, ec );

    fs::path output_path = tmp_dir / "trickle_data";

    // Successful run writes a fresh output file in the requested working directory.
    set_test_env( "CPDN_TIMED_PROCESS_SLEEP_MS", "100" );
    set_test_env( "CPDN_TIMED_PROCESS_WRITE_FILE", "trickle_data" );
    set_test_env( "CPDN_TIMED_PROCESS_EXIT_CODE", "0" );

    test_count++;
    TimedProcessResult success_result = run_process_with_timeout( TIMED_PROCESS_HELPER, {}, tmp_dir.string(), 2, output_path );
    if ( success_result.status == TimedProcessStatus::success && success_result.exit_code == 0 && success_result.output_updated &&
         fs::exists( output_path ) ) {
        test_passed++;
    } else {
        std::cerr << "  Expected successful timed process run with fresh output, got status="
                  << timed_process_status_to_string( success_result.status ) << ", exit_code=" << success_result.exit_code
                  << ", output_updated=" << success_result.output_updated << "\n";
    }

    // Non-zero exit should be reported as child_failed.
    set_test_env( "CPDN_TIMED_PROCESS_SLEEP_MS", "50" );
    clear_test_env( "CPDN_TIMED_PROCESS_WRITE_FILE" );
    set_test_env( "CPDN_TIMED_PROCESS_EXIT_CODE", "3" );

    test_count++;
    TimedProcessResult failure_result = run_process_with_timeout( TIMED_PROCESS_HELPER, {}, tmp_dir.string(), 2, output_path );
    if ( failure_result.status == TimedProcessStatus::child_failed && failure_result.exit_code == 3 ) {
        test_passed++;
    } else {
        std::cerr << "  Expected child_failed status with exit_code=3, got status="
                  << timed_process_status_to_string( failure_result.status ) << ", exit_code=" << failure_result.exit_code << "\n";
    }

    // Timeout should terminate the helper.
    set_test_env( "CPDN_TIMED_PROCESS_SLEEP_MS", "3000" );
    clear_test_env( "CPDN_TIMED_PROCESS_WRITE_FILE" );
    set_test_env( "CPDN_TIMED_PROCESS_EXIT_CODE", "0" );

    test_count++;
    TimedProcessResult timeout_result = run_process_with_timeout( TIMED_PROCESS_HELPER, {}, tmp_dir.string(), 1, output_path );
    if ( timeout_result.status == TimedProcessStatus::timed_out ) {
        test_passed++;
    } else {
        std::cerr << "  Expected timed_out status, got status=" << timed_process_status_to_string( timeout_result.status ) << "\n";
    }

    // Child environment variables should be applied in the forked child before exec.
    set_test_env( "CPDN_TIMED_PROCESS_SLEEP_MS", "0" );
    clear_test_env( "CPDN_TIMED_PROCESS_WRITE_FILE" );
    set_test_env( "CPDN_TIMED_PROCESS_EXIT_CODE", "0" );
    set_test_env( "CPDN_TIMED_PROCESS_CHECK_VAR_NAME", "CPDN_TEST_CHILD_ENV" );
    set_test_env( "CPDN_TIMED_PROCESS_CHECK_VAR_VALUE", "expected_value" );
    clear_test_env( "CPDN_TEST_CHILD_ENV" );

    test_count++;
    TimedProcessResult child_env_result = run_process_with_timeout( TIMED_PROCESS_HELPER,
                                                                    {},
                                                                    tmp_dir.string(),
                                                                    2,
                                                                    output_path,
                                                                    { { "CPDN_TEST_CHILD_ENV", "expected_value" } } );
    if ( child_env_result.status == TimedProcessStatus::success && child_env_result.exit_code == 0 ) {
        test_passed++;
    } else {
        std::cerr << "  Expected successful child environment setup, got status="
                  << timed_process_status_to_string( child_env_result.status ) << ", exit_code=" << child_env_result.exit_code << "\n";
    }

    clear_test_env( "CPDN_TIMED_PROCESS_SLEEP_MS" );
    clear_test_env( "CPDN_TIMED_PROCESS_WRITE_FILE" );
    clear_test_env( "CPDN_TIMED_PROCESS_EXIT_CODE" );
    clear_test_env( "CPDN_TIMED_PROCESS_CHECK_VAR_NAME" );
    clear_test_env( "CPDN_TIMED_PROCESS_CHECK_VAR_VALUE" );
    clear_test_env( "CPDN_TEST_CHILD_ENV" );

    fs::remove_all( tmp_dir, ec );

    std::cout << "  run_process_with_timeout: " << test_passed << "/" << test_count << " tests passed\n";
    if ( test_passed == test_count ) {
        SUCCESS;
        return EXIT_SUCCESS;
    } else {
        FAIL;
        return EXIT_FAILURE;
    }
}

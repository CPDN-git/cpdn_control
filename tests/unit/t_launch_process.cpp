/**
 * @file t_launch_process.cpp
 * @brief Unit test for launch_process() and check_child_status().
 */

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "../src/cpdn_control.h"
#include "process_control.h"
#include "unit_tests.h"

namespace fs = std::filesystem;

#ifndef LAUNCH_PROCESS_HELPER
#error "LAUNCH_PROCESS_HELPER is not defined"
#endif

namespace {
constexpr const char* kSleepEnv = "CPDN_LAUNCH_PROCESS_SLEEP_MS";

void set_test_env( const char* name, const std::string& value )
{
#if defined( _WIN32 )
    _putenv_s( name, value.c_str() );
#else
    setenv( name, value.c_str(), 1 );
#endif
}

void clear_test_env( const char* name )
{
#if defined( _WIN32 )
    _putenv_s( name, "" );
#else
    unsetenv( name );
#endif
}

bool wait_for_status( ChildProcessHandle& child_process, int expected_status, int timeout_seconds, bool& saw_running, int& exit_code )
{
    int process_status = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( timeout_seconds );

    while ( std::chrono::steady_clock::now() < deadline ) {
        process_status = check_child_status( child_process, process_status, exit_code );
        if ( process_status == 0 ) {
            saw_running = true;
        } else if ( process_status == expected_status ) {
            return true;
        } else {
            std::cerr << "  Unexpected process_status=" << process_status << " (expected " << expected_status << ")\n";
            return false;
        }
        std::this_thread::sleep_for( std::chrono::milliseconds( 200 ) );
    }

    std::cerr << "  Timeout waiting for process_status=" << expected_status << "\n";
    return false;
}
}    // namespace

int t_launch_process()
{
    TEST( "t_launch_process" );

    int test_count = 0;
    int test_passed = 0;

    fs::path tmp_dir = fs::temp_directory_path() / "cpdn_launch_process_test";
    std::error_code ec;
    fs::remove_all( tmp_dir, ec );
    fs::create_directories( tmp_dir, ec );

    const std::string project_path = tmp_dir.string();
    const std::string slot_path = tmp_dir.string();
    const std::string cmd = LAUNCH_PROCESS_HELPER;
    const std::string nthreads = "1";

    // Normal exit case.
    set_test_env( kSleepEnv, "200" );
    test_count++;
    ChildProcessHandle child_process = launch_process( project_path, slot_path, cmd, nthreads );
    if ( child_process_is_valid( child_process ) ) {
        test_passed++;
    } else {
        std::cerr << "  launch_process failed to start helper process\n";
    }

    test_count++;
    if ( child_process_is_valid( child_process ) ) {
        bool saw_running = false;
        int exit_code = 0;
        if ( wait_for_status( child_process, 1, 10, saw_running, exit_code ) && saw_running && exit_code == 0 ) {
            test_passed++;
        } else {
            std::cerr << "  Normal exit case did not behave as expected (exit_code=" << exit_code << ")\n";
        }
    } else {
        std::cerr << "  Skipping normal exit status check due to launch failure\n";
    }

    // Controller-driven termination case.
    set_test_env( kSleepEnv, "5000" );
    test_count++;
    child_process = launch_process( project_path, slot_path, cmd, nthreads );
    if ( !child_process_is_valid( child_process ) ) {
        std::cerr << "  launch_process failed to start helper process (termination case)\n";
    } else {
        BoincRuntime runtime{};
        runtime.client_status.abort_request = 1;
        if ( !handle_boinc_client_status( child_process, runtime ) ) {
            bool saw_running = false;
            int exit_code = 0;
            if ( wait_for_status( child_process, 3, 10, saw_running, exit_code ) && exit_code == -1 ) {
                test_passed++;
            } else {
                std::cerr << "  Controller termination case did not behave as expected (exit_code=" << exit_code << ")\n";
            }
        } else {
            std::cerr << "  handle_boinc_client_status should have requested task shutdown\n";
        }
    }

    // Suspend and resume case through the platform process-control seam.
    set_test_env( kSleepEnv, "5000" );
    test_count++;
    child_process = launch_process( project_path, slot_path, cmd, nthreads );
    if ( !child_process_is_valid( child_process ) ) {
        std::cerr << "  launch_process failed to start helper process (suspend/resume case)\n";
    } else {
        std::string err_msg;
        int exit_code = 0;
        if ( suspend_child_process( child_process, err_msg ) ) {
            int suspended_status = check_child_status( child_process, 0, exit_code );
            if ( suspended_status == 4 && resume_child_process( child_process, err_msg ) ) {
                if ( terminate_child_process( child_process, err_msg ) ) {
                    bool saw_running = false;
                    if ( wait_for_status( child_process, 3, 10, saw_running, exit_code ) ) {
                        test_passed++;
                    } else {
                        std::cerr << "  Suspend/resume cleanup did not reach terminated state\n";
                    }
                } else {
                    std::cerr << "  Failed to terminate helper after suspend/resume: " << err_msg << "\n";
                }
            } else {
                std::cerr << "  Suspend/resume case did not reach expected suspended/running states: " << err_msg << "\n";
            }
        } else {
            std::cerr << "  Failed to suspend helper process: " << err_msg << "\n";
        }
    }

    clear_test_env( kSleepEnv );
    fs::remove_all( tmp_dir, ec );

    std::cout << "  launch_process/check_child_status: " << test_passed << "/" << test_count << " tests passed\n";
    if ( test_passed == test_count ) {
        TEST_SUCCESS;
        return EXIT_SUCCESS;
    } else {
        TEST_FAIL;
        return EXIT_FAILURE;
    }
}

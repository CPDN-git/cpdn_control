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
#include <unistd.h>

#include "../src/cpdn_control.h"
#include "unit_tests.h"

namespace fs = std::filesystem;

#ifndef LAUNCH_PROCESS_HELPER
#error "LAUNCH_PROCESS_HELPER is not defined"
#endif

namespace {
constexpr const char* kSignalEnv = "CPDN_LAUNCH_PROCESS_SIGNAL";

bool wait_for_status( pid_t pid, int expected_status, int timeout_seconds, bool& saw_running, int& exit_code )
{
    int process_status = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( timeout_seconds );

    while ( std::chrono::steady_clock::now() < deadline ) {
        process_status = check_child_status( pid, process_status, exit_code );
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
    const std::string exptid = "test";

    // Normal exit case.
    test_count++;
    pid_t pid = launch_process( project_path, slot_path, cmd, nthreads, exptid );
    if ( pid > 0 ) {
        test_passed++;
    } else {
        std::cerr << "  launch_process failed to start helper process\n";
    }

    test_count++;
    if ( pid > 0 ) {
        bool saw_running = false;
        int exit_code = 0;
        if ( wait_for_status( pid, 1, 10, saw_running, exit_code ) && saw_running && exit_code == 0 ) {
            test_passed++;
        } else {
            std::cerr << "  Normal exit case did not behave as expected (exit_code=" << exit_code << ")\n";
        }
    } else {
        std::cerr << "  Skipping normal exit status check due to launch failure\n";
    }

    // Signal-termination case.
    std::string prev_value;
    const char* prev_env = std::getenv( kSignalEnv );
    if ( prev_env ) {
        prev_value = prev_env;
    }

    test_count++;
    if ( setenv( kSignalEnv, "TERM", 1 ) != 0 ) {
        std::cerr << "  Failed to set " << kSignalEnv << " environment variable\n";
    } else {
        pid = launch_process( project_path, slot_path, cmd, nthreads, exptid );
        if ( pid > 0 ) {
            bool saw_running = false;
            int exit_code = 0;
            if ( wait_for_status( pid, 3, 10, saw_running, exit_code ) && exit_code == -1 ) {
                test_passed++;
            } else {
                std::cerr << "  Signal termination case did not behave as expected (exit_code=" << exit_code << ")\n";
            }
        } else {
            std::cerr << "  launch_process failed to start helper process (signal case)\n";
        }
    }

    if ( !prev_value.empty() ) {
        setenv( kSignalEnv, prev_value.c_str(), 1 );
    } else {
        unsetenv( kSignalEnv );
    }

    fs::remove_all( tmp_dir, ec );

    std::cout << "  launch_process/check_child_status: " << test_passed << "/" << test_count << " tests passed\n";
    if ( test_passed == test_count ) {
        SUCCESS;
        return EXIT_SUCCESS;
    } else {
        FAIL;
        return EXIT_FAILURE;
    }
}

/**
 * @file t_launch_process.cpp
 * @brief Unit test for launch_process() and check_child_status().
 */

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <thread>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

#include "../api/model_control.h"
#include "../src/cpdn_control.h"
#include "process_control.h"
#include "unit_tests.h"

namespace fs = std::filesystem;

#ifndef LAUNCH_PROCESS_HELPER
#error "LAUNCH_PROCESS_HELPER is not defined"
#endif

namespace {
constexpr const char* kSleepEnv = "CPDN_LAUNCH_PROCESS_SLEEP_MS";
constexpr const char* kStdoutEnv = "CPDN_LAUNCH_PROCESS_STDOUT_TEXT";
constexpr const char* kStderrEnv = "CPDN_LAUNCH_PROCESS_STDERR_TEXT";

class LaunchProcessModelControl : public ModelControl {

  public:
    LaunchProcessModelControl() : ModelControl( "CPDN", "launch_process_test", "1.0", "helper" ) {}

    void print_logs( const int ) const override {}
    bool check_model_success() const override { return true; }
    bool restart_exists() const override { return false; }
    bool restart_ctl_read( std::string& step, std::string& time ) const override
    {
        step.clear();
        time.clear();
        return false;
    }
    ModelInputManifest get_input_manifest( const std::string& ) const override { return {}; }
    ModelControlInputData parse_control_input() const override { return {}; }
    bool get_current_step( int&, const int ) const override { return false; }
    std::vector<std::string> get_output_filenames( int ) const override { return {}; }
    std::vector<std::string> get_copyable_output_filenames( int ) const override { return {}; }
    bool is_output_filename( std::string_view ) const override { return false; }
    bool is_restart_filename( std::string_view ) const override { return false; }
    bool setup_directories( const fs::path& ) const override { return true; }
    std::vector<std::string> get_log_filenames() const override { return {}; }
};

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

#ifndef _WIN32
bool redirect_stderr_to_file( const fs::path& path, int& saved_stderr_fd, std::string& err_msg )
{
    err_msg.clear();
    saved_stderr_fd = dup( STDERR_FILENO );
    if ( saved_stderr_fd == -1 ) {
        err_msg = "dup(STDERR_FILENO) failed";
        return false;
    }

    int capture_fd = open( path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644 );
    if ( capture_fd == -1 ) {
        err_msg = "open(stderr capture file) failed";
        close( saved_stderr_fd );
        saved_stderr_fd = -1;
        return false;
    }

    if ( dup2( capture_fd, STDERR_FILENO ) == -1 ) {
        err_msg = "dup2(capture_fd, STDERR_FILENO) failed";
        close( capture_fd );
        close( saved_stderr_fd );
        saved_stderr_fd = -1;
        return false;
    }

    close( capture_fd );
    return true;
}

bool restore_stderr( int saved_stderr_fd, std::string& err_msg )
{
    err_msg.clear();
    if ( saved_stderr_fd == -1 ) {
        err_msg = "saved stderr fd is invalid";
        return false;
    }

    if ( dup2( saved_stderr_fd, STDERR_FILENO ) == -1 ) {
        err_msg = "dup2(saved_stderr_fd, STDERR_FILENO) failed";
        close( saved_stderr_fd );
        return false;
    }

    close( saved_stderr_fd );
    return true;
}
#endif
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
    LaunchProcessModelControl model_ctrl;

    // Normal exit case.
    set_test_env( kSleepEnv, "200" );
    test_count++;
    ChildProcessHandle child_process = launch_process( model_ctrl, project_path, slot_path, cmd, nthreads );
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

#ifndef _WIN32
    // Child stdout should be redirected into the inherited stderr stream.
    set_test_env( kSleepEnv, "0" );
    set_test_env( kStdoutEnv, "helper stdout line" );
    set_test_env( kStderrEnv, "helper stderr line" );
    test_count++;
    {
        fs::path stderr_capture = tmp_dir / "launch_process_stderr_capture.txt";
        int saved_stderr_fd = -1;
        std::string err_msg;
        std::string captured_output;

        if ( redirect_stderr_to_file( stderr_capture, saved_stderr_fd, err_msg ) ) {
            child_process = launch_process( model_ctrl, project_path, slot_path, cmd, nthreads );
            bool restored_ok = restore_stderr( saved_stderr_fd, err_msg );
            if ( !restored_ok ) {
                std::cerr << "  Failed to restore stderr after capture: " << err_msg << "\n";
            }

            if ( child_process_is_valid( child_process ) ) {
                bool saw_running = false;
                int exit_code = 0;
                if ( wait_for_status( child_process, 1, 10, saw_running, exit_code ) ) {
                    std::ifstream capture_in( stderr_capture );
                    captured_output.assign( std::istreambuf_iterator<char>( capture_in ), std::istreambuf_iterator<char>() );
                    if ( captured_output.find( "helper stdout line" ) != std::string::npos &&
                         captured_output.find( "helper stderr line" ) != std::string::npos ) {
                        test_passed++;
                    } else {
                        std::cerr << "  Expected helper stdout/stderr in captured stderr output, got: " << captured_output << "\n";
                    }
                } else {
                    std::cerr << "  Captured-output launch case did not exit normally\n";
                }
            } else {
                std::cerr << "  launch_process failed to start helper process (captured-output case)\n";
            }
        } else {
            std::cerr << "  Failed to redirect stderr for capture: " << err_msg << "\n";
        }
    }
    clear_test_env( kStdoutEnv );
    clear_test_env( kStderrEnv );
#endif

    // Controller-driven termination case.
    set_test_env( kSleepEnv, "5000" );
    test_count++;
    child_process = launch_process( model_ctrl, project_path, slot_path, cmd, nthreads );
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
    child_process = launch_process( model_ctrl, project_path, slot_path, cmd, nthreads );
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

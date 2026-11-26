#include "task_lifecycle.h"
#include "unit_tests.h"
#include "utils.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#ifdef __linux__
#include <sys/prctl.h>
#endif
#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
BOINC_STATUS next_status{};
int reads = 0;
int read_status( BOINC_STATUS* status )
{
    *status = next_status;
    ++reads;
    return 0;
}
int resume_status( BOINC_STATUS* status )
{
    *status = {};
    status->suspended = reads++ == 0;
    return 0;
}
int failed_status( BOINC_STATUS* ) { return -1; }

bool wait_for_file( const std::filesystem::path& path )
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 5 );
    while ( std::chrono::steady_clock::now() < deadline ) {
        std::ifstream file( path );
        unsigned long pid = 0;
        if ( file >> pid ) {
            return true;
        }
        std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
    }
    return false;
}
}    // namespace

int t_task_lifecycle()
{
    namespace fs = std::filesystem;
    using namespace std::chrono_literals;
    TEST( "t_task_lifecycle" );
    int failures = 0;
    auto expect = [&]( bool ok, const char* detail ) {
        if ( !ok ) {
            std::cerr << detail << '\n';
            ++failures;
        }
    };
    const auto dir = fs::temp_directory_path() / "cpdn_task_lifecycle_test";
    fs::remove_all( dir );
    fs::create_directories( dir );
    const auto executable = dir / fs::path( LAUNCH_PROCESS_HELPER ).filename();
    fs::copy_file( LAUNCH_PROCESS_HELPER, executable );

    // Exercise the same startup guard and final exit helper used by main().
    // Captured stderr is part of the volunteer-host debugging contract.
    std::ostringstream log;
    auto* saved_log = std::cerr.rdbuf( log.rdbuf() );
    for ( int request = 0; request < 3; ++request ) {
        next_status = {};
        if ( request == 0 ) {
            next_status.quit_request = 1;
        }
        if ( request == 1 ) {
            next_status.abort_request = 1;
        }
        if ( request == 2 ) {
            next_status.no_heartbeat = 1;
        }
        reads = 0;
        BoincRuntime runtime;
        expect( !check_boinc_before_launch( runtime, false, "before_launch", read_status ) && reads == 1, "Pending shutdown did not prevent launch" );
        TaskState task;
        const auto decision = make_boinc_shutdown_exit_decision( runtime );
        expect( finish_task_exit( task, decision ) == ( request == 1 ? 1 : 0 ), "Wrong BOINC exit status" );
        expect( !fs::exists( "boinc_finish_called" ), "Shutdown wrote a completion marker" );
    }
    BoincRuntime runtime;
    next_status = {};
    reads = 0;
    expect( check_boinc_before_launch( runtime, false, "before_launch", read_status ), "Clear startup status blocked launch" );
    expect( !check_boinc_before_launch( runtime, false, "before_launch", failed_status ), "Status read failure allowed launch" );
    reads = 0;
    expect( check_boinc_before_launch( runtime, false, "before_launch", resume_status ) && reads == 2, "Startup suspension did not wait for resume" );
    const int prior_reads = reads;
    runtime = {};
    expect( check_boinc_before_launch( runtime, true, "before_launch", read_status ) && reads == prior_reads,
            "Standalone startup read BOINC status" );
    std::cerr.rdbuf( saved_log );
    expect( log.str().find( "decision=shutdown" ) != std::string::npos && log.str().find( "decision=continue" ) != std::string::npos &&
                log.str().find( "wait_for_resume" ) != std::string::npos,
            "Missing startup decision logs" );

    // QUIT, ABORT and heartbeat loss after launch must use real final-route cleanup.
    for ( int request = 0; request < 3; ++request ) {
        TaskState task;
        std::string error;
        const auto ready_file = dir / ( "ready-" + std::to_string( request ) );
        task.child_process =
            start_child_process( executable.string(), dir.string(),
                                 { "CPDN_LAUNCH_PROCESS_SLEEP_MS=30000", "CPDN_LAUNCH_PROCESS_READY_FILE=" + ready_file.string() }, error );
        expect( child_process_is_valid( task.child_process ), "Cannot launch cleanup helper" );
        expect( wait_for_file( ready_file ), "Cleanup helper did not execute" );
        task.child_status = 0;
        if ( request == 1 ) {
            expect( suspend_child_process( task.child_process, error ), "Cannot suspend cleanup helper" );
        }
        runtime = {};
        if ( request == 0 ) {
            runtime.client_status.quit_request = 1;
        }
        if ( request == 1 ) {
            runtime.client_status.abort_request = 1;
        }
        if ( request == 2 ) {
            runtime.client_status.no_heartbeat = 1;
        }
        const auto decision = make_boinc_shutdown_exit_decision( runtime );
        expect( finish_task_exit( task, decision ) == ( request == 1 ? 1 : 0 ), "Unexpected exit code after launch" );
        expect( task.child_cleanup && task.child_cleanup->status == ChildCleanupStatus::terminated,
                "BOINC final exit did not confirm process termination" );
        // Immediate overwrite is the operation that failed in the remote reports.
        std::error_code ec;
        fs::copy_file( LAUNCH_PROCESS_HELPER, executable, fs::copy_options::overwrite_existing, ec );
        expect( !ec, "Executable still busy after BOINC shutdown" );
    }

    // A zero wait budget deterministically exercises unconfirmed termination.
    TaskState timed;
    std::string error;
    timed.child_process = start_child_process( executable.string(), dir.string(), { "CPDN_LAUNCH_PROCESS_SLEEP_MS=30000" }, error );
    expect( child_process_is_valid( timed.child_process ), "Cannot launch deadline helper" );
    timed.child_cleanup = shutdown_child_process( timed.child_process, 0ms );
    expect( timed.child_cleanup->status == ChildCleanupStatus::timed_out && timed.child_cleanup->elapsed < 500ms,
            "Cleanup deadline was not bounded" );
    const auto first_result = timed.child_cleanup->status;
    // Test teardown observes the child without granting production cleanup another budget.
    const auto teardown_deadline = std::chrono::steady_clock::now() + 2s;
    while ( poll_child_process_tree( timed.child_process, error ) == ChildTreeState::active &&
            std::chrono::steady_clock::now() < teardown_deadline ) {
        std::this_thread::sleep_for( 10ms );
    }
    finish_task_exit( timed, make_immediate_exit_decision( ExitReason::controller_error, 1 ) );
    expect( timed.child_cleanup->status == first_result, "Final exit restarted an exhausted cleanup budget" );

    // Keep an unrelated owned process alive while terminating another process set.
    auto unrelated = start_child_process( executable.string(), dir.string(), { "CPDN_LAUNCH_PROCESS_SLEEP_MS=30000" }, error );
    expect( child_process_is_valid( unrelated ), "Cannot launch unrelated helper" );
#ifdef __linux__
    // Reap test grandchildren ourselves, even in containers whose PID 1 does not reap.
    int prior_subreaper = 0;
    prctl( PR_GET_CHILD_SUBREAPER, &prior_subreaper );
    prctl( PR_SET_CHILD_SUBREAPER, 1 );
#endif
    for ( bool leader_exits : { false, true } ) {
        const auto pid_file = dir / ( leader_exits ? "exited-leader.pid" : "running-leader.pid" );
        ChildEnvironment env{ "CPDN_LAUNCH_PROCESS_SLEEP_MS=30000", "CPDN_LAUNCH_PROCESS_DESCENDANT_FILE=" + pid_file.string() };
        if ( leader_exits ) {
            env.push_back( "CPDN_LAUNCH_PROCESS_LEADER_EXIT=1" );
        }
        TaskState task;
        task.child_process = start_child_process( executable.string(), dir.string(), env, error );
        expect( child_process_is_valid( task.child_process ) && wait_for_file( pid_file ), "Descendant did not start" );
        unsigned long descendant = 0;
        std::ifstream( pid_file ) >> descendant;
        if ( leader_exits ) {
            const auto deadline = std::chrono::steady_clock::now() + 5s;
            int exit_code = -1;
            while ( poll_child_process( task.child_process, exit_code, error ) == ChildProcessState::running &&
                    std::chrono::steady_clock::now() < deadline ) {
                std::this_thread::sleep_for( 10ms );
            }
            expect( task.child_process.state == ChildProcessState::exited, "Leader did not exit before descendant cleanup" );
        }
#ifdef __linux__
        std::thread reap_descendant( [descendant] {
            if ( descendant == 0 ) {
                return;
            }
            const auto deadline = std::chrono::steady_clock::now() + 4s;
            while ( std::chrono::steady_clock::now() < deadline ) {
                if ( waitpid( static_cast<pid_t>( descendant ), nullptr, WNOHANG ) == static_cast<pid_t>( descendant ) ) {
                    return;
                }
                std::this_thread::sleep_for( 10ms );
            }
        } );
#endif
        finish_task_exit( task, make_immediate_exit_decision( ExitReason::controller_error, 1 ) );
#ifdef __linux__
        reap_descendant.join();
#endif
        expect( task.child_cleanup && task.child_cleanup->status == ChildCleanupStatus::terminated, "Descendants survived controller cleanup" );
        int exit_code = -1;
        expect( poll_child_process( unrelated, exit_code, error ) == ChildProcessState::running, "Cleanup affected a process from another launch" );
    }
    // Synchronous diagnostics/helpers obey the same descendant ownership contract.
    const auto helper_pid_file = dir / "timed-helper-descendant.pid";
#ifdef __linux__
    std::thread reap_helper_descendant( [&] {
        if ( !wait_for_file( helper_pid_file ) ) {
            return;
        }
        unsigned long pid = 0;
        std::ifstream( helper_pid_file ) >> pid;
        const auto deadline = std::chrono::steady_clock::now() + 4s;
        while ( pid != 0 && std::chrono::steady_clock::now() < deadline ) {
            if ( waitpid( static_cast<pid_t>( pid ), nullptr, WNOHANG ) == static_cast<pid_t>( pid ) ) {
                return;
            }
            std::this_thread::sleep_for( 10ms );
        }
    } );
#endif
    const auto helper_result =
        run_process_with_timeout( executable.string(), {}, dir.string(), 5, {},
                                  { "CPDN_LAUNCH_PROCESS_DESCENDANT_FILE=" + helper_pid_file.string(), "CPDN_LAUNCH_PROCESS_LEADER_EXIT=1" } );
#ifdef __linux__
    reap_helper_descendant.join();
#endif
    expect( helper_result.status == TimedProcessStatus::success && helper_result.cleanup.status == ChildCleanupStatus::terminated,
            "Synchronous helper left descendants after leader exit" );
#ifdef __linux__
    prctl( PR_SET_CHILD_SUBREAPER, prior_subreaper );
#endif
    shutdown_child_process( unrelated );
    close_child_process_handle( unrelated );
    fs::remove_all( dir );
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

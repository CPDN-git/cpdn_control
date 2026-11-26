#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using ProcessId = std::uint64_t;
using ChildEnvironment = std::vector<std::string>;

enum class ChildProcessState { running, exited, terminated, suspended, unavailable };

/** @brief Explicit ownership of a launch, retained after leader exit until cleanup and close.
 *  POSIX keeps a waitable leader to reserve PID/PGID; Windows keeps process/job handles.
 *  Validity denotes ownership, not whether the leader is still running.
 */
struct ChildProcessHandle {
    ProcessId process_id = 0;
    ProcessId process_group_id = 0;    // POSIX group owned by this launch, never the controller's group.
    std::uintptr_t native_process_handle = 0;
    std::uintptr_t native_job_handle = 0;
    bool suspended = false;
    bool termination_requested = false;
    ChildProcessState state = ChildProcessState::running;
    int exit_code = 0;
    bool reaped = false;
};

enum class ChildCleanupStatus { already_exited, terminated, timed_out, failed };

/** @brief Outcome of stopping an owned process set; signal submission alone is not confirmation. */
struct ChildCleanupResult {
    ChildCleanupStatus status = ChildCleanupStatus::already_exited;
    bool termination_requested = false;
    std::chrono::milliseconds elapsed{ 0 };
    std::string message;
};

enum class ChildTreeState { active, gone, unavailable };

bool child_process_is_valid( const ChildProcessHandle& child_process );
ChildProcessHandle start_child_process( const std::string& executable, const std::string& working_dir, const ChildEnvironment& env_vars,
                                        std::string& err_msg );
ChildProcessState poll_child_process( ChildProcessHandle& child_process, int& exit_code, std::string& err_msg );
bool terminate_child_process( ChildProcessHandle& child_process, std::string& err_msg );
/** @brief Probe the owned process set after termination is requested; also reap the direct child when possible. */
ChildTreeState poll_child_process_tree( ChildProcessHandle& child_process, std::string& err_msg );
/** @brief Stop an owned process group/job and confirm exit within one bounded monotonic wait. */
ChildCleanupResult shutdown_child_process( ChildProcessHandle& child_process, std::chrono::milliseconds timeout = std::chrono::milliseconds( 2000 ) );
const char* child_cleanup_status_to_string( ChildCleanupStatus status );
bool suspend_child_process( ChildProcessHandle& child_process, std::string& err_msg );
bool resume_child_process( ChildProcessHandle& child_process, std::string& err_msg );
void close_child_process_handle( ChildProcessHandle& child_process );

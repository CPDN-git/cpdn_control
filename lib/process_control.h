#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using ProcessId = std::uint64_t;
using ChildEnvironment = std::vector<std::pair<std::string, std::string>>;

enum class ChildProcessState {
    running,
    exited,
    terminated,
    suspended,
    unavailable
};

struct ChildProcessHandle {
    ProcessId process_id = 0;
    std::uintptr_t native_process_handle = 0;
    std::uintptr_t native_job_handle = 0;
    bool suspended = false;
    bool termination_requested = false;
};

bool child_process_is_valid( const ChildProcessHandle& child_process );
ChildProcessHandle start_child_process( const std::string& executable, const std::string& working_dir, const ChildEnvironment& env_vars,
                                       std::string& err_msg );
ChildProcessState poll_child_process( ChildProcessHandle& child_process, int& exit_code, std::string& err_msg );
bool terminate_child_process( ChildProcessHandle& child_process, std::string& err_msg );
bool suspend_child_process( ChildProcessHandle& child_process, std::string& err_msg );
bool resume_child_process( ChildProcessHandle& child_process, std::string& err_msg );
void close_child_process_handle( ChildProcessHandle& child_process );

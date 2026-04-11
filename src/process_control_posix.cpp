#ifndef _WIN32

#include "process_control.h"

#include <cerrno>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace {

std::vector<std::string> build_child_environment( const ChildEnvironment& env_vars )
{
    std::map<std::string, std::string> merged_env;

    for ( char** entry = environ; entry != nullptr && *entry != nullptr; ++entry ) {
        std::string env_entry = *entry;
        auto sep = env_entry.find( '=' );
        if ( sep == std::string::npos ) {
            continue;
        }
        merged_env[env_entry.substr( 0, sep )] = env_entry.substr( sep + 1 );
    }

    for ( const auto& [name, value] : env_vars ) {
        merged_env[name] = value;
    }

    std::vector<std::string> env_storage;
    env_storage.reserve( merged_env.size() );
    for ( const auto& [name, value] : merged_env ) {
        env_storage.push_back( name + "=" + value );
    }
    return env_storage;
}

bool send_signal( ProcessId process_id, int signal_number, std::string& err_msg )
{
    err_msg.clear();
    if ( process_id == 0 ) {
        err_msg = "child process is not valid";
        return false;
    }

    if ( kill( static_cast<pid_t>( process_id ), signal_number ) == 0 ) {
        return true;
    }

    if ( errno == ESRCH ) {
        return true;
    }

    err_msg = std::strerror( errno );
    return false;
}

}    // namespace


bool child_process_is_valid( const ChildProcessHandle& child_process ) { return child_process.process_id != 0; }


ChildProcessHandle start_child_process( const std::string& executable, const std::string& working_dir, const ChildEnvironment& env_vars,
                                       std::string& err_msg )
{
    err_msg.clear();
    ChildProcessHandle child_process;

    if ( executable.empty() ) {
        err_msg = "child executable path is empty";
        return child_process;
    }

    std::vector<std::string> env_storage = build_child_environment( env_vars );

    pid_t pid = fork();
    if ( pid == -1 ) {
        err_msg = std::strerror( errno );
        return child_process;
    }

    if ( pid == 0 ) {
        if ( !working_dir.empty() && chdir( working_dir.c_str() ) != 0 ) {
            _exit( 126 );
        }

        struct rlimit core_limits;
        core_limits.rlim_cur = core_limits.rlim_max = 0;
        if ( setrlimit( RLIMIT_CORE, &core_limits ) != 0 ) {
            _exit( 125 );
        }

#ifndef __APPLE__
        struct rlimit stack_limits;
        stack_limits.rlim_cur = stack_limits.rlim_max = RLIM_INFINITY;
        if ( setrlimit( RLIMIT_STACK, &stack_limits ) != 0 ) {
            _exit( 124 );
        }
#endif

        std::vector<char*> argv;
        argv.push_back( const_cast<char*>( executable.c_str() ) );
        argv.push_back( nullptr );

        std::vector<char*> envp;
        envp.reserve( env_storage.size() + 1 );
        for ( auto& entry : env_storage ) {
            envp.push_back( entry.data() );
        }
        envp.push_back( nullptr );

        execve( executable.c_str(), argv.data(), envp.data() );
        _exit( 127 );
    }

    child_process.process_id = static_cast<ProcessId>( pid );
    return child_process;
}


int poll_child_process( ChildProcessHandle& child_process, int child_status, int& exit_code, std::string& err_msg )
{
    (void)child_status;
    err_msg.clear();

    if ( !child_process_is_valid( child_process ) ) {
        err_msg = "child process handle is not valid";
        return 5;
    }

    int stat = 0;
    pid_t wait_result = waitpid( static_cast<pid_t>( child_process.process_id ), &stat, WNOHANG | WUNTRACED | WCONTINUED );
    if ( wait_result == 0 ) {
        return child_process.suspended ? 4 : 0;
    }
    if ( wait_result == -1 ) {
        err_msg = std::strerror( errno );
        return 5;
    }

    if ( WIFEXITED( stat ) ) {
        exit_code = WEXITSTATUS( stat );
        close_child_process_handle( child_process );
        return 1;
    }
    if ( WIFSIGNALED( stat ) ) {
        exit_code = -1;
        close_child_process_handle( child_process );
        return 3;
    }
    if ( WIFSTOPPED( stat ) ) {
        child_process.suspended = true;
        exit_code = -1;
        return 4;
    }
#ifdef WIFCONTINUED
    if ( WIFCONTINUED( stat ) ) {
        child_process.suspended = false;
        return 0;
    }
#endif

    return child_process.suspended ? 4 : 0;
}


bool terminate_child_process( ChildProcessHandle& child_process, std::string& err_msg )
{
    child_process.termination_requested = true;
    return send_signal( child_process.process_id, SIGKILL, err_msg );
}


bool suspend_child_process( ChildProcessHandle& child_process, std::string& err_msg )
{
    if ( !send_signal( child_process.process_id, SIGSTOP, err_msg ) ) {
        return false;
    }
    child_process.suspended = true;
    return true;
}


bool resume_child_process( ChildProcessHandle& child_process, std::string& err_msg )
{
    if ( !send_signal( child_process.process_id, SIGCONT, err_msg ) ) {
        return false;
    }
    child_process.suspended = false;
    return true;
}


void close_child_process_handle( ChildProcessHandle& child_process )
{
    child_process.process_id = 0;
    child_process.native_process_handle = 0;
    child_process.suspended = false;
    child_process.termination_requested = false;
}

#endif

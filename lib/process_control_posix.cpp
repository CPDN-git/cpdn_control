#ifndef _WIN32

#include "process_control.h"

#include <cerrno>
#include <cstdio>
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

bool parse_env_entry( const std::string& env_entry, std::string& name, std::string& value )
{
    auto sep = env_entry.find( '=' );
    if ( sep == std::string::npos || sep == 0 || env_entry.find( '\0' ) != std::string::npos ) {
        return false;
    }

    name = env_entry.substr( 0, sep );
    value = env_entry.substr( sep + 1 );
    return value.find( '\0' ) == std::string::npos;
}

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

    for ( const auto& env_entry : env_vars ) {
        std::string name;
        std::string value;
        if ( !parse_env_entry( env_entry, name, value ) ) {
            return {};
        }
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
    if ( !env_vars.empty() && env_storage.empty() ) {
        err_msg = "invalid child environment entry";
        return child_process;
    }
    std::vector<char*> argv;
    argv.reserve( 2 );
    argv.push_back( const_cast<char*>( executable.c_str() ) );
    argv.push_back( nullptr );

    std::vector<char*> envp;
    envp.reserve( env_storage.size() + 1 );
    for ( auto& entry : env_storage ) {
        envp.push_back( entry.data() );
    }
    envp.push_back( nullptr );

    const char* executable_path = executable.c_str();
    const char* working_dir_path = working_dir.c_str();

    pid_t pid = fork();
    if ( pid == -1 ) {
        err_msg = std::strerror( errno );
        return child_process;
    }

    if ( pid == 0 ) {
        if ( setpgid( 0, 0 ) != 0 ) {
            _exit( 121 );
        }
        if ( working_dir_path[0] != '\0' && chdir( working_dir_path ) != 0 ) {
            _exit( 126 );
        }

        // Redirect only the model child stdout into stdout.txt in the slot directory.
        // The inherited stderr stream remains unchanged so controller and BOINC stderr
        // handling still apply there.
        FILE* stdout_file = freopen( "stdout.txt", "a", stdout );
        if ( stdout_file == nullptr ) {
            _exit( 122 );
        }
        setvbuf( stdout_file, nullptr, _IOLBF, BUFSIZ );

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

        execve( executable_path, argv.data(), envp.data() );
        _exit( 127 );
    }

    child_process.process_id = static_cast<ProcessId>( pid );
    // Both parent and child establish the group so an immediate shutdown cannot
    // race the child entering its group. The child always does so before exec.
    if ( setpgid( pid, pid ) != 0 && getpgid( pid ) != pid ) {
        const std::string group_error = std::strerror( errno );
        (void)shutdown_child_process( child_process );
        close_child_process_handle( child_process );
        err_msg = "cannot establish model process group: " + group_error;
        return child_process;
    }
    child_process.process_group_id = static_cast<ProcessId>( pid );
    return child_process;
}


ChildProcessState poll_child_process( ChildProcessHandle& child_process, int& exit_code, std::string& err_msg )
{
    err_msg.clear();

    if ( !child_process_is_valid( child_process ) ) {
        err_msg = "child process handle is not valid";
        return ChildProcessState::unavailable;
    }

    if ( child_process.state == ChildProcessState::exited || child_process.state == ChildProcessState::terminated ) {
        exit_code = child_process.exit_code;
        return child_process.state;
    }

    siginfo_t info{};
    // Observe exits without reaping. Keeping the leader waitable reserves its
    // PID/PGID until shutdown has signalled any surviving descendants.
    if ( waitid( P_PID, static_cast<id_t>( child_process.process_id ), &info, WEXITED | WSTOPPED | WCONTINUED | WNOHANG | WNOWAIT ) != 0 ) {
        if ( errno == EINTR ) {
            return child_process.state;
        }
        err_msg = std::strerror( errno );
        return ChildProcessState::unavailable;
    }
    if ( info.si_pid == 0 ) {
        return child_process.state;
    }

    switch ( info.si_code ) {
    case CLD_EXITED:
        child_process.exit_code = info.si_status;
        child_process.state = ChildProcessState::exited;
        break;
    case CLD_KILLED:
    case CLD_DUMPED:
        child_process.exit_code = -1;
        child_process.state = ChildProcessState::terminated;
        break;
    case CLD_STOPPED:
    case CLD_TRAPPED:
        child_process.suspended = true;
        child_process.state = ChildProcessState::suspended;
        break;
    case CLD_CONTINUED:
        child_process.suspended = false;
        child_process.state = ChildProcessState::running;
        break;
    }
    exit_code = child_process.exit_code;
    return child_process.state;
}


bool terminate_child_process( ChildProcessHandle& child_process, std::string& err_msg )
{
    err_msg.clear();
    if ( child_process.termination_requested ) {
        return true;
    }
    if ( child_process.process_group_id != 0 ) {
        const auto group = static_cast<pid_t>( child_process.process_group_id );
        if ( group <= 1 || group == getpgrp() ) {
            err_msg = "refusing to signal a process group not isolated from the controller";
            return false;
        }
        if ( kill( -group, SIGKILL ) != 0 && errno != ESRCH ) {
            err_msg = std::strerror( errno );
            return false;
        }
    } else if ( !send_signal( child_process.process_id, SIGKILL, err_msg ) ) {
        return false;
    }
    child_process.termination_requested = true;
    return true;
}


ChildTreeState poll_child_process_tree( ChildProcessHandle& child_process, std::string& err_msg )
{
    int exit_code = 0;
    auto state = poll_child_process( child_process, exit_code, err_msg );
    if ( state == ChildProcessState::unavailable ) {
        return ChildTreeState::unavailable;
    }
    if ( state != ChildProcessState::exited && state != ChildProcessState::terminated ) {
        return ChildTreeState::active;
    }
    if ( !child_process.reaped ) {
        const auto reaped_pid = waitpid( static_cast<pid_t>( child_process.process_id ), nullptr, WNOHANG );
        if ( reaped_pid == 0 || ( reaped_pid < 0 && errno == EINTR ) ) {
            return ChildTreeState::active;
        }
        if ( reaped_pid < 0 ) {
            err_msg = std::strerror( errno );
            return ChildTreeState::unavailable;
        }
        child_process.reaped = true;
    }
    if ( child_process.process_group_id == 0 ) {
        return ChildTreeState::gone;
    }
    if ( kill( -static_cast<pid_t>( child_process.process_group_id ), 0 ) == 0 ) {
        return ChildTreeState::active;
    }
    if ( errno == ESRCH ) {
        return ChildTreeState::gone;
    }
    err_msg = std::strerror( errno );
    return ChildTreeState::unavailable;
}


bool suspend_child_process( ChildProcessHandle& child_process, std::string& err_msg )
{
    if ( !send_signal( child_process.process_id, SIGSTOP, err_msg ) ) {
        return false;
    }
    child_process.suspended = true;
    child_process.state = ChildProcessState::suspended;
    return true;
}


bool resume_child_process( ChildProcessHandle& child_process, std::string& err_msg )
{
    if ( !send_signal( child_process.process_id, SIGCONT, err_msg ) ) {
        return false;
    }
    child_process.suspended = false;
    child_process.state = ChildProcessState::running;
    return true;
}


void close_child_process_handle( ChildProcessHandle& child_process )
{
    if ( child_process.process_id != 0 && !child_process.reaped ) {
        // Never block here, including when cleanup exhausted its deadline.
        (void)waitpid( static_cast<pid_t>( child_process.process_id ), nullptr, WNOHANG );
    }
    child_process = {};
}

#endif

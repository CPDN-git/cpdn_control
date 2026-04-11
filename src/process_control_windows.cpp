#ifdef _WIN32

#include "process_control.h"

#include <algorithm>
#include <map>
#include <string>
#include <system_error>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>

namespace {

std::wstring utf8_to_wide( const std::string& text )
{
    if ( text.empty() ) {
        return {};
    }

    int size = MultiByteToWideChar( CP_UTF8, 0, text.c_str(), -1, nullptr, 0 );
    if ( size <= 0 ) {
        return {};
    }

    std::wstring wide_text( static_cast<std::size_t>( size ), L'\0' );
    MultiByteToWideChar( CP_UTF8, 0, text.c_str(), -1, wide_text.data(), size );
    wide_text.pop_back();
    return wide_text;
}


std::string get_windows_error_message( DWORD error_code )
{
    return std::system_category().message( static_cast<int>( error_code ) );
}


std::vector<wchar_t> build_environment_block( const ChildEnvironment& env_vars )
{
    std::vector<std::wstring> special_entries;
    std::map<std::wstring, std::wstring> merged_env;

    if ( LPWCH current_env = GetEnvironmentStringsW(); current_env != nullptr ) {
        LPWCH cursor = current_env;
        while ( *cursor != L'\0' ) {
            std::wstring entry = cursor;
            cursor += entry.size() + 1;

            if ( !entry.empty() && entry.front() == L'=' ) {
                special_entries.push_back( entry );
                continue;
            }

            auto sep = entry.find( L'=' );
            if ( sep == std::wstring::npos ) {
                continue;
            }

            merged_env[entry.substr( 0, sep )] = entry.substr( sep + 1 );
        }
        FreeEnvironmentStringsW( current_env );
    }

    for ( const auto& [name, value] : env_vars ) {
        merged_env[utf8_to_wide( name )] = utf8_to_wide( value );
    }

    std::vector<wchar_t> env_block;
    auto append_entry = [&env_block]( const std::wstring& entry ) {
        env_block.insert( env_block.end(), entry.begin(), entry.end() );
        env_block.push_back( L'\0' );
    };

    for ( const auto& entry : special_entries ) {
        append_entry( entry );
    }
    for ( const auto& [name, value] : merged_env ) {
        append_entry( name + L"=" + value );
    }
    env_block.push_back( L'\0' );
    return env_block;
}


bool apply_thread_action( ProcessId process_id, bool suspend_threads, std::string& err_msg )
{
    err_msg.clear();

    HANDLE snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
    if ( snapshot == INVALID_HANDLE_VALUE ) {
        err_msg = get_windows_error_message( GetLastError() );
        return false;
    }

    THREADENTRY32 entry{};
    entry.dwSize = sizeof( entry );

    bool touched_any_thread = false;
    if ( Thread32First( snapshot, &entry ) ) {
        do {
            if ( entry.th32OwnerProcessID != static_cast<DWORD>( process_id ) ) {
                continue;
            }

            HANDLE thread_handle = OpenThread( THREAD_SUSPEND_RESUME, FALSE, entry.th32ThreadID );
            if ( thread_handle == nullptr ) {
                continue;
            }

            DWORD result = suspend_threads ? SuspendThread( thread_handle ) : ResumeThread( thread_handle );
            CloseHandle( thread_handle );

            if ( result == static_cast<DWORD>( -1 ) ) {
                CloseHandle( snapshot );
                err_msg = get_windows_error_message( GetLastError() );
                return false;
            }
            touched_any_thread = true;
        } while ( Thread32Next( snapshot, &entry ) );
    }

    CloseHandle( snapshot );

    if ( !touched_any_thread ) {
        err_msg = "no child threads were available";
        return false;
    }

    return true;
}

}    // namespace


bool child_process_is_valid( const ChildProcessHandle& child_process )
{
    return child_process.process_id != 0 && child_process.native_process_handle != 0;
}


ChildProcessHandle start_child_process( const std::string& executable, const std::string& working_dir, const ChildEnvironment& env_vars,
                                       std::string& err_msg )
{
    err_msg.clear();
    ChildProcessHandle child_process;

    std::wstring executable_w = utf8_to_wide( executable );
    if ( executable_w.empty() ) {
        err_msg = "child executable path is empty or invalid UTF-8";
        return child_process;
    }

    std::wstring working_dir_w = utf8_to_wide( working_dir );
    std::wstring command_line = L"\"" + executable_w + L"\"";
    std::vector<wchar_t> command_line_buffer( command_line.begin(), command_line.end() );
    command_line_buffer.push_back( L'\0' );
    std::vector<wchar_t> env_block = build_environment_block( env_vars );

    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof( startup_info );
    PROCESS_INFORMATION process_info{};

    if ( !CreateProcessW( executable_w.c_str(),
                          command_line_buffer.data(),
                          nullptr,
                          nullptr,
                          FALSE,
                          CREATE_UNICODE_ENVIRONMENT,
                          env_block.data(),
                          working_dir_w.empty() ? nullptr : working_dir_w.c_str(),
                          &startup_info,
                          &process_info ) ) {
        err_msg = get_windows_error_message( GetLastError() );
        return child_process;
    }

    CloseHandle( process_info.hThread );

    child_process.process_id = static_cast<ProcessId>( process_info.dwProcessId );
    child_process.native_process_handle = reinterpret_cast<std::uintptr_t>( process_info.hProcess );
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

    HANDLE process_handle = reinterpret_cast<HANDLE>( child_process.native_process_handle );
    DWORD wait_result = WaitForSingleObject( process_handle, 0 );
    if ( wait_result == WAIT_TIMEOUT ) {
        return child_process.suspended ? 4 : 0;
    }
    if ( wait_result != WAIT_OBJECT_0 ) {
        err_msg = get_windows_error_message( GetLastError() );
        return 5;
    }

    DWORD process_exit_code = 0;
    if ( !GetExitCodeProcess( process_handle, &process_exit_code ) ) {
        err_msg = get_windows_error_message( GetLastError() );
        return 5;
    }

    bool termination_requested = child_process.termination_requested;
    close_child_process_handle( child_process );
    if ( termination_requested ) {
        exit_code = -1;
        return 3;
    }

    exit_code = static_cast<int>( process_exit_code );
    return 1;
}


bool terminate_child_process( ChildProcessHandle& child_process, std::string& err_msg )
{
    err_msg.clear();
    if ( !child_process_is_valid( child_process ) ) {
        err_msg = "child process handle is not valid";
        return false;
    }

    HANDLE process_handle = reinterpret_cast<HANDLE>( child_process.native_process_handle );
    if ( !TerminateProcess( process_handle, 1 ) ) {
        err_msg = get_windows_error_message( GetLastError() );
        return false;
    }

    child_process.termination_requested = true;
    return true;
}


bool suspend_child_process( ChildProcessHandle& child_process, std::string& err_msg )
{
    if ( !child_process_is_valid( child_process ) ) {
        err_msg = "child process handle is not valid";
        return false;
    }

    if ( !apply_thread_action( child_process.process_id, true, err_msg ) ) {
        return false;
    }

    child_process.suspended = true;
    return true;
}


bool resume_child_process( ChildProcessHandle& child_process, std::string& err_msg )
{
    if ( !child_process_is_valid( child_process ) ) {
        err_msg = "child process handle is not valid";
        return false;
    }

    if ( !apply_thread_action( child_process.process_id, false, err_msg ) ) {
        return false;
    }

    child_process.suspended = false;
    return true;
}


void close_child_process_handle( ChildProcessHandle& child_process )
{
    if ( child_process.native_process_handle != 0 ) {
        CloseHandle( reinterpret_cast<HANDLE>( child_process.native_process_handle ) );
    }

    child_process.process_id = 0;
    child_process.native_process_handle = 0;
    child_process.suspended = false;
    child_process.termination_requested = false;
}

#endif

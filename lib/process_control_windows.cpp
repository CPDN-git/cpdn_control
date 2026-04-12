#ifdef _WIN32

#include "process_control.h"

#include <algorithm>
#include <cstddef>
#include <cwctype>
#include <map>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <TlHelp32.h>

namespace {

struct WideCaseInsensitiveLess {
    bool operator()( const std::wstring& lhs, const std::wstring& rhs ) const
    {
        return std::lexicographical_compare( lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), []( wchar_t left, wchar_t right ) {
            return std::towlower( left ) < std::towlower( right );
        } );
    }
};

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


bool contains_embedded_nul( const std::string& text ) { return text.find( '\0' ) != std::string::npos; }


bool is_valid_env_name( const std::string& name )
{
    return !name.empty() && name.find( '=' ) == std::string::npos && !contains_embedded_nul( name );
}


std::string get_windows_error_message( DWORD error_code )
{
    return std::system_category().message( static_cast<int>( error_code ) );
}


bool build_environment_block( const ChildEnvironment& env_vars, std::vector<wchar_t>& env_block, std::string& err_msg )
{
    std::vector<std::wstring> special_entries;
    std::map<std::wstring, std::wstring, WideCaseInsensitiveLess> merged_env;
    env_block.clear();
    err_msg.clear();

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
        if ( !is_valid_env_name( name ) || contains_embedded_nul( value ) ) {
            err_msg = "invalid child environment entry";
            return false;
        }
        merged_env[utf8_to_wide( name )] = utf8_to_wide( value );
    }

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
    return true;
}


bool create_job_object( HANDLE& job_handle, std::string& err_msg )
{
    err_msg.clear();
    job_handle = CreateJobObjectW( nullptr, nullptr );
    if ( job_handle == nullptr ) {
        err_msg = get_windows_error_message( GetLastError() );
        return false;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit_info{};
    limit_info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if ( !SetInformationJobObject( job_handle, JobObjectExtendedLimitInformation, &limit_info, sizeof( limit_info ) ) ) {
        err_msg = get_windows_error_message( GetLastError() );
        CloseHandle( job_handle );
        job_handle = nullptr;
        return false;
    }
    return true;
}


bool get_job_process_ids( HANDLE job_handle, std::vector<DWORD>& process_ids, std::string& err_msg )
{
    err_msg.clear();
    process_ids.clear();
    DWORD capacity = 8;

    while ( true ) {
        std::size_t buffer_size =
            offsetof( JOBOBJECT_BASIC_PROCESS_ID_LIST, ProcessIdList ) + ( static_cast<std::size_t>( capacity ) * sizeof( ULONG_PTR ) );
        std::vector<std::byte> buffer( buffer_size );
        auto* process_list = reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST*>( buffer.data() );

        if ( QueryInformationJobObject( job_handle, JobObjectBasicProcessIdList, process_list, static_cast<DWORD>( buffer.size() ), nullptr ) ) {
            process_ids.reserve( process_list->NumberOfProcessIdsInList );
            for ( DWORD i = 0; i < process_list->NumberOfProcessIdsInList; ++i ) {
                process_ids.push_back( static_cast<DWORD>( process_list->ProcessIdList[i] ) );
            }
            return true;
        }

        DWORD last_error = GetLastError();
        if ( last_error != ERROR_MORE_DATA ) {
            err_msg = get_windows_error_message( last_error );
            return false;
        }
        capacity *= 2;
    }
}


bool collect_job_thread_ids( HANDLE job_handle, std::vector<DWORD>& thread_ids, std::string& err_msg )
{
    err_msg.clear();
    thread_ids.clear();

    std::vector<DWORD> process_ids;
    if ( !get_job_process_ids( job_handle, process_ids, err_msg ) ) {
        return false;
    }
    if ( process_ids.empty() ) {
        err_msg = "no child processes were available";
        return false;
    }

    std::set<DWORD> process_id_set( process_ids.begin(), process_ids.end() );

    HANDLE snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
    if ( snapshot == INVALID_HANDLE_VALUE ) {
        err_msg = get_windows_error_message( GetLastError() );
        return false;
    }

    THREADENTRY32 entry{};
    entry.dwSize = sizeof( entry );

    if ( Thread32First( snapshot, &entry ) ) {
        do {
            if ( process_id_set.find( entry.th32OwnerProcessID ) == process_id_set.end() ) {
                continue;
            }
            thread_ids.push_back( entry.th32ThreadID );
        } while ( Thread32Next( snapshot, &entry ) );
    }

    CloseHandle( snapshot );

    if ( thread_ids.empty() ) {
        err_msg = "no child threads were available";
        return false;
    }

    std::sort( thread_ids.begin(), thread_ids.end() );
    thread_ids.erase( std::unique( thread_ids.begin(), thread_ids.end() ), thread_ids.end() );
    return true;
}


bool apply_thread_action( HANDLE job_handle, bool suspend_threads, std::string& err_msg )
{
    err_msg.clear();
    constexpr int kSuspendRetryLimit = 3;

    for ( int attempt = 0; attempt < kSuspendRetryLimit; ++attempt ) {
        std::vector<DWORD> thread_ids;
        if ( !collect_job_thread_ids( job_handle, thread_ids, err_msg ) ) {
            return false;
        }

        std::vector<DWORD> processed_thread_ids;
        processed_thread_ids.reserve( thread_ids.size() );

        for ( DWORD thread_id : thread_ids ) {
            HANDLE thread_handle = OpenThread( THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION, FALSE, thread_id );
            if ( thread_handle == nullptr ) {
                DWORD last_error = GetLastError();
                if ( last_error == ERROR_INVALID_PARAMETER ) {
                    continue;
                }
                err_msg = get_windows_error_message( last_error );
                return false;
            }

            DWORD result = suspend_threads ? SuspendThread( thread_handle ) : ResumeThread( thread_handle );
            DWORD thread_error = GetLastError();
            CloseHandle( thread_handle );

            if ( result == static_cast<DWORD>( -1 ) ) {
                err_msg = get_windows_error_message( thread_error );
                return false;
            }
            processed_thread_ids.push_back( thread_id );
        }

        if ( !suspend_threads ) {
            return true;
        }

        std::vector<DWORD> remaining_thread_ids;
        if ( !collect_job_thread_ids( job_handle, remaining_thread_ids, err_msg ) ) {
            return false;
        }

        bool stable_thread_set = true;
        for ( DWORD thread_id : remaining_thread_ids ) {
            if ( std::find( processed_thread_ids.begin(), processed_thread_ids.end(), thread_id ) == processed_thread_ids.end() ) {
                stable_thread_set = false;
                break;
            }
        }

        if ( stable_thread_set ) {
            return true;
        }
    }

    err_msg = "child thread set kept changing while suspending";
    return false;
}

}    // namespace


bool child_process_is_valid( const ChildProcessHandle& child_process )
{
    return child_process.process_id != 0 && child_process.native_process_handle != 0 && child_process.native_job_handle != 0;
}


ChildProcessHandle start_child_process( const std::string& executable, const std::string& working_dir, const ChildEnvironment& env_vars,
                                       std::string& err_msg )
{
    err_msg.clear();
    ChildProcessHandle child_process;

    if ( contains_embedded_nul( executable ) || contains_embedded_nul( working_dir ) ) {
        err_msg = "child launch path contains embedded NUL bytes";
        return child_process;
    }

    std::wstring executable_w = utf8_to_wide( executable );
    if ( executable_w.empty() ) {
        err_msg = "child executable path is empty or invalid UTF-8";
        return child_process;
    }

    std::wstring working_dir_w = utf8_to_wide( working_dir );
    std::wstring command_line = L"\"" + executable_w + L"\"";
    std::vector<wchar_t> command_line_buffer( command_line.begin(), command_line.end() );
    command_line_buffer.push_back( L'\0' );

    std::vector<wchar_t> env_block;
    if ( !build_environment_block( env_vars, env_block, err_msg ) ) {
        return child_process;
    }

    HANDLE job_handle = nullptr;
    if ( !create_job_object( job_handle, err_msg ) ) {
        return child_process;
    }

    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof( startup_info );
    PROCESS_INFORMATION process_info{};

    if ( !CreateProcessW( executable_w.c_str(),
                          command_line_buffer.data(),
                          nullptr,
                          nullptr,
                          FALSE,
                          CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED,
                          env_block.data(),
                          working_dir_w.empty() ? nullptr : working_dir_w.c_str(),
                          &startup_info,
                          &process_info ) ) {
        err_msg = get_windows_error_message( GetLastError() );
        CloseHandle( job_handle );
        return child_process;
    }

    if ( !AssignProcessToJobObject( job_handle, process_info.hProcess ) ) {
        err_msg = get_windows_error_message( GetLastError() );
        TerminateProcess( process_info.hProcess, 1 );
        CloseHandle( process_info.hThread );
        CloseHandle( process_info.hProcess );
        CloseHandle( job_handle );
        return child_process;
    }

    if ( ResumeThread( process_info.hThread ) == static_cast<DWORD>( -1 ) ) {
        err_msg = get_windows_error_message( GetLastError() );
        TerminateJobObject( job_handle, 1 );
        CloseHandle( process_info.hThread );
        CloseHandle( process_info.hProcess );
        CloseHandle( job_handle );
        return child_process;
    }

    CloseHandle( process_info.hThread );

    child_process.process_id = static_cast<ProcessId>( process_info.dwProcessId );
    child_process.native_process_handle = reinterpret_cast<std::uintptr_t>( process_info.hProcess );
    child_process.native_job_handle = reinterpret_cast<std::uintptr_t>( job_handle );
    return child_process;
}


ChildProcessState poll_child_process( ChildProcessHandle& child_process, int& exit_code, std::string& err_msg )
{
    err_msg.clear();

    if ( !child_process_is_valid( child_process ) ) {
        err_msg = "child process handle is not valid";
        return ChildProcessState::unavailable;
    }

    HANDLE process_handle = reinterpret_cast<HANDLE>( child_process.native_process_handle );
    DWORD wait_result = WaitForSingleObject( process_handle, 0 );
    if ( wait_result == WAIT_TIMEOUT ) {
        return child_process.suspended ? ChildProcessState::suspended : ChildProcessState::running;
    }
    if ( wait_result != WAIT_OBJECT_0 ) {
        err_msg = get_windows_error_message( GetLastError() );
        return ChildProcessState::unavailable;
    }

    DWORD process_exit_code = 0;
    if ( !GetExitCodeProcess( process_handle, &process_exit_code ) ) {
        err_msg = get_windows_error_message( GetLastError() );
        return ChildProcessState::unavailable;
    }

    bool termination_requested = child_process.termination_requested;
    close_child_process_handle( child_process );
    if ( termination_requested ) {
        exit_code = -1;
        return ChildProcessState::terminated;
    }

    exit_code = static_cast<int>( process_exit_code );
    return ChildProcessState::exited;
}


bool terminate_child_process( ChildProcessHandle& child_process, std::string& err_msg )
{
    err_msg.clear();
    if ( !child_process_is_valid( child_process ) ) {
        err_msg = "child process handle is not valid";
        return false;
    }

    HANDLE job_handle = reinterpret_cast<HANDLE>( child_process.native_job_handle );
    if ( !TerminateJobObject( job_handle, 1 ) ) {
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

    HANDLE job_handle = reinterpret_cast<HANDLE>( child_process.native_job_handle );
    if ( !apply_thread_action( job_handle, true, err_msg ) ) {
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

    HANDLE job_handle = reinterpret_cast<HANDLE>( child_process.native_job_handle );
    if ( !apply_thread_action( job_handle, false, err_msg ) ) {
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
    if ( child_process.native_job_handle != 0 ) {
        CloseHandle( reinterpret_cast<HANDLE>( child_process.native_job_handle ) );
    }

    child_process.process_id = 0;
    child_process.native_process_handle = 0;
    child_process.native_job_handle = 0;
    child_process.suspended = false;
    child_process.termination_requested = false;
}

#endif

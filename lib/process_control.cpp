#include "process_control.h"

#include <algorithm>
#include <thread>

ChildCleanupResult shutdown_child_process( ChildProcessHandle& child_process, std::chrono::milliseconds timeout )
{
    ChildCleanupResult result;
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::max( timeout, std::chrono::milliseconds::zero() );
    auto finish = [&]( ChildCleanupStatus status ) {
        result.status = status;
        result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now() - start );
        return result;
    };

    if ( !child_process_is_valid( child_process ) ) {
        return finish( ChildCleanupStatus::already_exited );
    }

    // Signal before reaping: on POSIX the unreaped leader reserves the group identity.
    if ( !terminate_child_process( child_process, result.message ) ) {
        return finish( ChildCleanupStatus::failed );
    }
    result.termination_requested = true;

    while ( std::chrono::steady_clock::now() < deadline ) {
        auto state = poll_child_process_tree( child_process, result.message );
        if ( state == ChildTreeState::gone ) {
            return finish( ChildCleanupStatus::terminated );
        }
        if ( state == ChildTreeState::unavailable ) {
            return finish( ChildCleanupStatus::failed );
        }
        std::this_thread::sleep_until( std::min( deadline, std::chrono::steady_clock::now() + std::chrono::milliseconds( 20 ) ) );
    }

    result.message = "owned process set still present or exit unconfirmed at cleanup deadline";
    return finish( ChildCleanupStatus::timed_out );
}

const char* child_cleanup_status_to_string( ChildCleanupStatus status )
{
    switch ( status ) {
    case ChildCleanupStatus::already_exited:
        return "already_exited";
    case ChildCleanupStatus::terminated:
        return "termination_confirmed";
    case ChildCleanupStatus::timed_out:
        return "deadline_expired";
    case ChildCleanupStatus::failed:
        return "termination_failed";
    }
    return "unknown";
}

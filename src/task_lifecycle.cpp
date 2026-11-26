#include "task_lifecycle.h"

#include <iostream>

static const char* exit_reason_to_string( const ExitReason reason )
{
    switch ( reason ) {
    case ExitReason::loop_completed:
        return "loop_completed";
    case ExitReason::boinc_quit:
        return "boinc_quit";
    case ExitReason::boinc_abort:
        return "boinc_abort";
    case ExitReason::boinc_no_heartbeat:
        return "boinc_no_heartbeat";
    case ExitReason::startup_error:
        return "startup_error";
    case ExitReason::controller_error:
        return "controller_error";
    case ExitReason::upload_error:
        return "upload_error";
    }
    return "unknown";
}


static const char* exit_route_to_string( const ExitRoute route )
{
    switch ( route ) {
    case ExitRoute::immediate:
        return "immediate";
    case ExitRoute::controller_error_recovery:
        return "controller_error_recovery";
    case ExitRoute::boinc_shutdown:
        return "boinc_shutdown";
    case ExitRoute::task_completed:
        return "task_completed";
    }
    return "unknown";
}

/**
 * @brief Construct a non-completion exit that stops an active child and leaves immediately.
 */
ExitDecision make_immediate_exit_decision( const ExitReason reason, const int exit_code ) { return { reason, exit_code, ExitRoute::immediate }; }


/**
 * @brief Construct a non-completion exit after controller recovery has attempted child cleanup.
 */
ExitDecision make_recovered_exit_decision( const ExitReason reason, const int exit_code )
{
    return { reason, exit_code, ExitRoute::controller_error_recovery };
}


/**
 * @brief Construct the explicit controller exit decision for a BOINC-driven shutdown.
 *        QUIT and heartbeat loss remain restartable; abort takes a non-finish failure path.
 */
ExitDecision make_boinc_shutdown_exit_decision( const BoincRuntime& runtime )
{
    ExitReason reason = ExitReason::controller_error;    // default failure

    // Classify the BOINC shutdown request into a controller exit reason for logging.
    if ( runtime.client_status.quit_request ) {
        reason = ExitReason::boinc_quit;
    } else if ( runtime.client_status.abort_request ) {
        reason = ExitReason::boinc_abort;
    } else if ( runtime.client_status.no_heartbeat ) {
        reason = ExitReason::boinc_no_heartbeat;
    }

    // QUIT and heartbeat loss return zero without recording task completion.
    if ( reason == ExitReason::boinc_quit || reason == ExitReason::boinc_no_heartbeat ) {
        return { reason, 0, ExitRoute::boinc_shutdown };
    }
    // BOINC has asked to abort the task.
    return { reason, 1, ExitRoute::boinc_shutdown };
}

/**
 * @brief Construct the genuine task-completion exit decision.
 *        This is the only path that is allowed to call boinc_finish().
 */
ExitDecision make_completion_exit_decision( const bool model_success )
{
    return { ExitReason::loop_completed, model_success ? 0 : 1, ExitRoute::task_completed };
}


/**
 * @brief Stop the model child process if it is still active during controller shutdown.
 */
void stop_child_for_shutdown( TaskState& tstate, std::string_view reason )
{
    if ( tstate.child_cleanup.has_value() ) {
        return;    // Recovery and final exit share one cleanup attempt and deadline.
    }
    const auto pid = tstate.child_process.process_id;
    const auto group = tstate.child_process.process_group_id;
    std::cerr << "Child cleanup: reason=" << reason << ", pid=" << pid << ", group=" << group << ", checking owned process set\n";
    tstate.child_cleanup = shutdown_child_process( tstate.child_process );
    const auto& result = *tstate.child_cleanup;
    if ( tstate.child_process.state == ChildProcessState::exited ) {
        tstate.child_status = 1;
    } else if ( tstate.child_process.state == ChildProcessState::terminated ) {
        tstate.child_status = 3;
    }
    std::cerr << "Child cleanup: reason=" << reason << ", pid=" << pid << ", group=" << group
              << ", result=" << child_cleanup_status_to_string( result.status ) << ", termination_requested=" << result.termination_requested
              << ", elapsed_ms=" << result.elapsed.count();
    if ( !result.message.empty() ) {
        std::cerr << ", detail=" << result.message;
    }
    std::cerr << '\n';
}


/**
 * @brief Take the controller's single final-exit path.
 *        Every route attempts bounded owned-process cleanup before closing handles and ending
 *        the critical section. Only natural task completion may call boinc_finish().
 */
int finish_task_exit( TaskState& tstate, const ExitDecision& decision )
{
    // Every controlled exit owns cleanup, including restartable BOINC shutdown.
    // Recovery may already have used the single bounded cleanup attempt.
    stop_child_for_shutdown( tstate, exit_reason_to_string( decision.reason ) );

    // Log the exit decision and key task state to stderr for debugging and post-mortem analysis.
    std::cerr << "Exit decision: reason=" << exit_reason_to_string( decision.reason ) << ", route=" << exit_route_to_string( decision.route )
              << ", exit_code=" << decision.exit_code << ", child_status=" << tstate.child_status << ", model_completed=" << tstate.model_completed
              << ", model_success=" << ( tstate.model_success ? "true" : "false" ) << '\n';

    close_child_process_handle( tstate.child_process );
    boinc_end_critical_section();    // in case we abort while in critical section (boinc api handles case if not in critical section).
    if ( decision.route == ExitRoute::task_completed ) {
        boinc_finish( decision.exit_code );    // boinc_finish exits, no further code executed after this call (unless a dummy library is used).
    }
    return decision.exit_code;
}

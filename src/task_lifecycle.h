#pragma once

#include <string_view>

#include "cpdn_control.h"

/** @brief Controller exit cause and cleanup route, independent of child lifecycle state. */
enum class ExitReason {
    loop_completed,
    boinc_quit,
    boinc_abort,
    boinc_no_heartbeat,
    startup_error,
    controller_error,
    upload_error,
};

/**
 * @brief Defines the cleanup policy for a controller exit independently from its cause.
 */
enum class ExitRoute {
    immediate,
    controller_error_recovery,
    boinc_shutdown,
    task_completed,
};

struct ExitDecision {
    ExitReason reason;
    int exit_code;
    ExitRoute route;
};

ExitDecision make_immediate_exit_decision( ExitReason reason, int exit_code );
ExitDecision make_recovered_exit_decision( ExitReason reason, int exit_code );
ExitDecision make_boinc_shutdown_exit_decision( const BoincRuntime& runtime );
ExitDecision make_completion_exit_decision( bool model_success );
/** @brief Attempt owned child cleanup once per controller run, with a total two-second wait budget. */
void stop_child_for_shutdown( TaskState& tstate, std::string_view reason );
/** @brief Clean up every exit route; only genuine completion may call boinc_finish(). */
int finish_task_exit( TaskState& tstate, const ExitDecision& decision );

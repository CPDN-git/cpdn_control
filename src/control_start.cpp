#include "control_start.h"

#include <sstream>
#include <string>

#include "../lib/utils.h"

TaskStartupStateResult initialize_task_state_from_restart( ModelControl& model_ctrl, const ProgressFileHandler& progress_file,
                                                           const int restart_interval_steps, TaskState& tstate, std::string& err_msg )
{
    err_msg.clear();

    const bool progress_file_exists = progress_file.exists();
    const bool restart_exists = model_ctrl.restart_exists();
    const bool progress_file_is_empty = progress_file_exists ? progress_file.is_empty() : false;

    if ( !progress_file_exists && !restart_exists ) {
        return { true, TaskStartupMode::fresh_run, "-- Starting new model run --\n", false };
    }

    if ( progress_file_exists && !progress_file_is_empty && restart_exists ) {
        std::string restart_step;
        if ( !model_ctrl.parse_restart( restart_step ) ) {
            err_msg = "Parsing model restart metadata failed";
            return { false, TaskStartupMode::invalid, "", true };
        }

        if ( !progress_file.read( tstate, err_msg ) ) {
            err_msg = "Failed to read progress file: " + err_msg;
            return { false, TaskStartupMode::invalid, "", false };
        }

        int restart_step_value = 0;
        std::string restart_step_text = restart_step;
        if ( !parse_int( restart_step_text, restart_step_value, err_msg ) ) {
            err_msg = "Failed to parse restart STEP value: " + err_msg;
            return { false, TaskStartupMode::invalid, "", false };
        }

        if ( restart_step_value > tstate.last_completed_step ) {
            err_msg = "STEP variable from model restart greater than last_completed_step from progress file, error occurred. Exiting..";
            return { false, TaskStartupMode::invalid, "", false };
        }

        std::ostringstream log_message;
        log_message << "-- Model is restarting --\n"
                    << "Adjusting last_completed_step, " << tstate.last_completed_step << ", to previous model restart step.\n";

        int adjusted_restart_step = tstate.last_completed_step;
        adjusted_restart_step = adjusted_restart_step - ( ( adjusted_restart_step % restart_interval_steps ) -
                                                          1 );    // -1 because the model will continue from restart_step.
        tstate.last_completed_step = adjusted_restart_step;

        return { true, TaskStartupMode::restart_run, log_message.str(), false };
    }

    if ( progress_file_exists && progress_file_is_empty ) {
        err_msg = "progress file exists, but is empty => problem with model, quitting run";
        return { false, TaskStartupMode::invalid, "", true };
    }

    if ( progress_file_exists && !restart_exists ) {
        if ( !progress_file.read( tstate, err_msg ) ) {
            err_msg = "Failed to read progress file: " + err_msg;
            return { false, TaskStartupMode::invalid, "", false };
        }

        if ( tstate.last_completed_step >= restart_interval_steps ) {
            err_msg = "progress file exists, but rcf file does not exist => problem with model, quitting run";
            return { false, TaskStartupMode::invalid, "", true };
        }

        return { true, TaskStartupMode::fresh_run, "", false };
    }

    if ( !progress_file_exists && restart_exists ) {
        err_msg = "rcf file exists, but progress file does not exist => problem with task, quitting run";
        return { false, TaskStartupMode::invalid, "", true };
    }

    err_msg = "Unexpected restart/progress-file state";
    return { false, TaskStartupMode::invalid, "", false };
}

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

    //  GlennC. In the past, OpenIFS could run alone if the control code crashed. This meant on a restart
    //  the step count from the model's output could be way higher than the count in the progress file.
    //  To fix this, code is now addded to the model to check the controller process is still running and
    //  if not abort. However, due to delays in flushing file output, the model log can still be a few
    //  steps ahead of the progress file. To account for this, we allow a small difference between the
    //  restart step and the last_completed_step in the progress file.  Any more than this and we
    //  assume the model has run standalone and the task should be aborted.
    const int allowed_restart_step_diff = 5;

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

        if ( restart_step_value > tstate.last_completed_step + allowed_restart_step_diff ) {
            err_msg = "Timestep count from model restart is much higher than last_completed_step in the progress file, error occurred. Exiting..";
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


void prepare_task_state_for_controller_run( TaskState& tstate )
{
    // The progress file may come from a previously completed controller process.
    // Clear transient controller-run state before launching a new child so the main-loop gate is valid.
    tstate.model_completed = 0;
    tstate.current_step = tstate.last_completed_step;
    tstate.current_cpu_time = tstate.prior_acc_cpu_time;
}

#pragma once

#include <string>

#include "../api/model_control.h"
#include "../api/progressfile_handler.h"
#include "cpdn_control.h"

enum class TaskStartupMode { fresh_run, restart_run, invalid };

struct TaskStartupStateResult {
    bool ok = false;
    TaskStartupMode startup_mode = TaskStartupMode::invalid;
    std::string log_message;
    bool print_model_logs = false;
};

TaskStartupStateResult initialize_task_state_from_restart( ModelControl& model_ctrl, const ProgressFileHandler& progress_file,
                                                           int restart_interval_steps, TaskState& tstate, std::string& err_msg );

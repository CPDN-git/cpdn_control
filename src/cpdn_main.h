/**
 * @file cpdn_main.h
 * @brief Header file for CPDN main program definitions
 * 
 * Contains the TaskState struct that encapsulates all task-related state variables
 * for managing model execution, and forward declarations for progress file functions.
 * 
 *    Glenn Carver, CPDN, 2025.
 */

#pragma once

#include <string>

/**
 * @struct TaskState
 * @brief Encapsulates all task-related state variables for managing model execution.
 *        Groups logically related variables for better code organization and clarity.
 */
struct TaskState {
    int last_cpu_time = 0;          // CPU time at last checkpoint
    int upload_file_number = 0;     // Sequential counter for upload files
    std::string last_iter = "0";    // Last completed iteration step
    int last_upload = 0;            // Time of last upload file (in seconds)
    int model_completed = 0;        // Model completion state: 0=running, 1=completed
    int current_iter = 0;           // Current iteration step (in seconds)
    int last_trickle_iter = 0;      // Last iteration when trickle was sent
    int process_status = 1;         // Child process status: 0=running, 1=stopped, etc.
    double current_cpu_time = 0.0;  // Current accumulated CPU time
    double fraction_done = 0.0;     // Fraction of model run completed (0.0-1.0)
};


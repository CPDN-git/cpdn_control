#pragma once

#include <filesystem>
#include <string>
#include <vector>

class ModelControl;

/**
 * @brief Run diagnostics.exe synchronously in the slot directory for the completed step.
 *        Builds the current experimental argument list from the matching ICMSH filename and only leaves trickle_data in place if the run succeeds.
 */
bool run_step_diagnostics( const ModelControl& model_ctrl, const std::filesystem::path& diag_exe, const std::filesystem::path& slot_path,
                           const std::vector<std::string>& output_files );

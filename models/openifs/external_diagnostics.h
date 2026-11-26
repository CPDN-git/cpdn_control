#pragma once

#include <filesystem>
#include <string>
#include <vector>

class ModelControl;
struct TimedProcessResult;

/**
 * @brief Accept diagnostics output only after successful execution, safe cleanup, and a fresh output file.
 *        Logs rejected results and removes their trickle_data; returns true only when output is accepted.
 */
bool accept_diagnostics_result( const TimedProcessResult& result, const std::filesystem::path& trickle_data_path );

/**
 * @brief Run diagnostics.exe synchronously in the slot directory for the completed step.
 *        Builds the current experimental argument list from the matching ICMSH filename and validates execution, cleanup, and output freshness.
 *        Returns true when diagnostics were attempted, even if their output is rejected; diagnostics failure remains nonfatal to the model.
 */
bool run_step_diagnostics( const ModelControl& model_ctrl, const std::filesystem::path& diag_exe, const std::filesystem::path& slot_path,
                           const std::vector<std::string>& output_files );

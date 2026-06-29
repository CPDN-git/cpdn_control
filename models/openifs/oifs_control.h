//
// OpenIFS model control class header.
//  Glenn Carver, CPDN, 2025.

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "../../api/model_control.h"


namespace fs = std::filesystem;


class OpenIFSControl : public ModelControl {

  public:
    // Constructor and destructor methods
    OpenIFSControl( std::string_view vendor, std::string_view model, std::string_view version, std::string_view exe )
        : ModelControl( vendor, model, version, exe )
    {
    }
    ~OpenIFSControl() override = default;

    // Public interface methods
    // Overrides of virtual functions in ModelControl

    void print_logs( const int nlines ) const override;
    bool check_model_success() const override;

    bool setup( const fs::path& slot_path ) const override;
    bool do_step_tasks( int current_step, const fs::path& slot_path ) override;

    // Getters and setters

    ModelInputManifest get_input_manifest( const std::string& wu ) const override;
    std::vector<std::string> get_env_vars( const std::string& slot_path, const std::string& nthreads, std::string& err_msg ) const override;
    bool get_current_step( int& step, const int total_steps ) const override;
    std::vector<std::string> get_output_filenames( int step ) const override;
    std::vector<std::string> get_copyable_output_filenames( int current_step ) const override;
    bool is_output_filename( std::string_view filename ) const override;
    bool is_restart_filename( std::string_view filename ) const override;
    std::vector<std::string> get_log_filenames() const override;

    // Gives the minimum and maximum number of threads the model can use based on the model configuration and/or system resources.
    void get_nthreads_range( int& min_threads, int& max_threads ) const override
    {
        min_threads = 1;
        max_threads = 8;    // More than 8 threads is deemed too inefficient, even at T319.
    }

    // Model specific methods
    bool setup_directories( const fs::path& slot_path ) const override;

    ModelControlInputData parse_control_input() const override;

    bool restart_exists() const override;
    bool parse_restart( std::string& step ) const override;


    // Delete copy constructor and assignment operator

    OpenIFSControl( const OpenIFSControl& ) = delete;
    OpenIFSControl& operator=( const OpenIFSControl& ) = delete;


  private:
    // Private member variables
    const fs::path control_input_file{ "fort.4" };

    const fs::path ifs_stat{ "ifs.stat" };

    const fs::path rcf{ "rcf" };

    const std::vector<std::string> log_files{ "NODE.001_01", "ifs.stat", "rcf", "waminfo" };

    // OpenIFS input data directories where files are unpacked.
    const fs::path ifsdata_dir{ "ifsdata" };
    const fs::path climdata_dir{ "climdata" };

    // OpenIFS experiment ID, assigned by reading the namelist control_input_file.
    // mutable so can change in parse_control_input() which is a const method as it does not
    // change the returned state of ModelControlInputData, but held internally in this class only.
    mutable std::string experiment_id;
    mutable int output_interval = 0;    // Cached model output cadence in steps for model-owned copyability logic.

    // External diagnostics executable name, if present.
    mutable std::string diag_exe_name{ "diagnostics.exe" };
    mutable fs::path diag_exe_path{};
    int last_step_tasks_step = -1;
};

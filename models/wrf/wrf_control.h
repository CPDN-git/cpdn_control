//
// WRF 4.6.1 Urban configuration control class header.
//   Glenn Carver, CPDN, 2026.

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "../../api/model_control.h"


class WRFControl : public ModelControl {

  public:
    // Constructor and destructor methods
    WRFControl( std::string_view vendor, std::string_view model, std::string_view version, std::string_view exe )
        : ModelControl( vendor, model, version, exe )
    {
    }
    ~WRFControl() override = default;

    // Public interface methods
    // overrides of pure virtual functions in ModelControl

    void print_logs( const int nlines ) const override;
    bool check_model_success() const override;

    // Getters and setters

    ModelInputManifest get_input_manifest( const std::string& wu ) const override;
    std::vector<std::string> get_env_vars( const std::string& slot_path, const std::string& nthreads, std::string& err_msg ) const override;
    bool get_current_step( int& step, const int total_steps ) const override;
    std::vector<std::string> get_output_filenames( int step, std::string_view id ) const override;
    bool is_output_filename( std::string_view filename ) const override;
    bool is_restart_filename( std::string_view filename ) const override;
    std::vector<std::string> get_log_filenames() const override;

    // Gives the minimum and maximum number of threads the model can use based on the model configuration and/or system resources.
    void get_nthreads_range( int& min_threads, int& max_threads ) const override
    {
        min_threads = 1;
        max_threads = 16;
    }

    // Model specific methods
    bool setup_directories( const fs::path& slot_path ) const override;

    ModelControlInputData parse_control_input() const override;

    // TODO: check how WRF handles restarts.
    bool restart_ctl_exists() const override;
    bool restart_ctl_read( std::string& step, std::string& time ) const override;

    // Delete copy constructor and assignment operator
    WRFControl( const WRFControl& ) = delete;
    WRFControl& operator=( const WRFControl& ) = delete;
};

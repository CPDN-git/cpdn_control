//
// WRF 4.6.1  configuration control class header.
//   Glenn Carver, CPDN, 2026.

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "../../api/model_control.h"


namespace fs = std::filesystem;

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

    bool setup( const fs::path& slot_path ) const override;
    bool do_step_tasks( int current_step, const fs::path& slot_path ) override;

    // Getters and setters

    ModelInputManifest get_input_manifest( const std::string& wu ) const override;
    std::vector<std::string> get_env_vars( const std::string& slot_path, const std::string& nthreads, std::string& err_msg ) const override;
    bool get_current_step( int& step, const int total_steps ) const override;
    std::vector<std::string> get_output_filenames( int step ) const override;
    bool is_output_filename( std::string_view filename ) const override;
    bool is_restart_filename( std::string_view filename ) const override;
    std::vector<std::string> get_log_filenames() const override;

    // Gives the minimum and maximum number of threads the model can use based on the model configuration and/or system resources.
    void get_nthreads_range( int& min_threads, int& max_threads ) const override
    {
        min_threads = 1;
        max_threads = 12;    // tests show OpenMP efficiency dropss off significantly above 8
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


  private:
    // Private helper variables

    // WRF control namelist file
    const fs::path control_input_file{ "namelist.input" };    // WRF control input file

    // Timestep in seconds
    // Used to contruct time period differences
    mutable int timestep_seconds = 0;

    // Start date and time for the run read from namelist.input.
    // Needed to construct time periods for output and restart files.
    mutable int start_year = 0;
    mutable int start_month = 0;
    mutable int start_day = 0;
    mutable int start_hour = 0;
    mutable int start_min = 0;
    mutable int start_sec = 0;    // this should never be anything other than zero!

    // Number of domains this WRF configuration is set up to run.
    // This is used to determine how many output and restart files to expect.
    mutable int max_domains = 0;
};

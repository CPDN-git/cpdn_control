//
// WRF 4.6.1  configuration control class header.
//   Glenn Carver, CPDN, 2026.

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "../../api/model_control.h"
#include "wrf_datetime.h"


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
    std::vector<std::string> get_copyable_output_filenames( int current_step ) const override;
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

    bool restart_exists() const override;
    bool parse_restart( std::string& step ) const override;

    // Delete copy constructor and assignment operator
    WRFControl( const WRFControl& ) = delete;
    WRFControl& operator=( const WRFControl& ) = delete;


  private:
    //  We support a maximum of max_dom_allowed nests; in practise 3 is usually the max.
    static constexpr int max_dom_allowed = 3;

    struct RestartSet {
        std::string datetime;
        std::vector<bool> domains_present;
    };

    // Private helper variables

    // WRF control namelist file
    const fs::path control_input_file{ "namelist.input" };                // WRF control input file
    const fs::path control_input_file_step0{ "namelist.input.step0" };    // WRF control input file for step 0 (used for restart)

    //  WRF writes to stdout.txt as control code redirects. We read this to determine success and upload it.
    const fs::path model_log{ "stdout.txt" };

    // Timestep in seconds
    // Used to contruct time period differences
    mutable int timestep_seconds = 0;
    mutable int output_interval = 0;    // Cached model output cadence in steps for model-owned copyability logic.

    // Start date and time for the run read from namelist.input.
    // Needed to construct time periods for output and restart files.
    mutable DateTime step0_datetime{};    // this is the date/time of the very start, read from namelist.input.step0.
    mutable DateTime restart_reference_start_datetime{};
    mutable bool restart_reference_start_valid = false;

    // Number of domains this WRF configuration is set up to run.
    // This is used to determine how many output and restart files to expect.
    mutable int max_domains = 0;
    mutable std::vector<std::string> output_prefixes;
    mutable std::vector<std::string> restart_prefixes;
    mutable std::vector<RestartSet> restart_sets;
    mutable bool restart_scan_cached = false;
    mutable bool cached_restart_exists = false;
    mutable fs::path cached_restart_scan_dir;
    mutable int cached_restart_scan_max_domains = 0;

    // Private class helper functions
    bool read_validated_max_domains( int& parsed_max_domains, std::string& err_msg ) const;
    bool ensure_domain_prefixes_initialized() const;
    void set_domain_prefixes( int domain_count ) const;
    void clear_restart_scan_cache() const;
    const RestartSet* find_latest_valid_restart_set() const;
    bool update_restart_namelist( const RestartSet& restart_set ) const;
};

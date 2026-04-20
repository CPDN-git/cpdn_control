//
// OpenIFS model control class header.
//  Glenn Carver, CPDN, 2025.

#pragma once

#include <filesystem>
#include <regex>
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
    // overrides of pure virtual functions in ModelControl

    void print_logs( const int nlines ) const override;
    bool check_model_success() const override;

    // Getters and setters

    ModelInputManifest get_input_manifest( const std::string& wu ) const override;
    bool get_current_step( int& step, const int total_steps ) const override;
    std::vector<std::string> get_output_filenames( int step, std::string_view id ) const override;
    std::vector<std::string> get_log_filenames() const override;
    std::regex get_output_filename_regex() const override;

    // Model specific methods
    bool setup_directories( const fs::path& slot_path ) const override;

    ModelControlInputData parse_control_input() const override;

    bool restart_ctl_exists() const override;
    bool restart_ctl_read( std::string& step, std::string& time ) const override;


    // Delete copy constructor and assignment operator

    OpenIFSControl( const OpenIFSControl& ) = delete;
    OpenIFSControl& operator=( const OpenIFSControl& ) = delete;


  private:
    // Private member variables
    const fs::path control_input_file{ "fort.4" };

    const fs::path ifs_stat{ "ifs.stat" };

    const fs::path rcf{ "rcf" };

    const std::vector<std::string> log_files{ "NODE.001_01", "ifs.stat", "rcf", "waminfo" };

    const std::regex output_file_pattern{ R"(^ICM[A-Za-z]{6}\+[0-9]{6}$)" };

    // OpenIFS input data directories where files are unpacked.
    const fs::path ifsdata_dir{ "ifsdata" };
    const fs::path climdata_dir{ "climdata" };
};

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
    OpenIFSControl( std::string_view vendor, std::string_view model, std::string_view version, std::string_view param_infile )
        : ModelControl( vendor, model, version, param_infile )
    {
    }
    ~OpenIFSControl() override = default;

    // Public interface methods
    // overrides of pure virtual functions in ModelControl

    void print_logs( const int nlines ) const override;
    bool check_model_success() const override;

    // Getters and setters

    bool get_current_step( std::string& step, const int total_steps ) const override;
    std::vector<std::string> get_output_filenames( std::string_view step, std::string_view id ) const override;
    std::vector<std::string> get_log_filenames() const override;
    std::regex get_output_filename_regex() const override;


    // Delete copy constructor and assignment operator

    OpenIFSControl( const OpenIFSControl& ) = delete;
    OpenIFSControl& operator=( const OpenIFSControl& ) = delete;


  private:
    // Private member variables
    const fs::path ifs_stat{ "ifs_stat" };

    const std::vector<std::string> log_files{ "NODE.001_01", "ifs.stat", "rcf", "waminfo" };

    const std::regex output_file_pattern{ R"(^ICM[A-Za-z]{6}\+[0-9]{6}$)" };
};
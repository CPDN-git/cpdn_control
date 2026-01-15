//
// Test model control class header.
//  Glenn Carver, CPDN, 2025.

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "../../api/model_control.h"


namespace fs = std::filesystem;


class TestControl : public ModelControl {

  public:
    // Constructor and destructor methods
    TestControl( std::string_view vendor, std::string_view model, std::string_view version, std::string_view input_file )
        : ModelControl( vendor, model, version, input_file )
    {
    }
    ~TestControl() override = default;

    // Public interface methods
    // overrides of pure virtual functions in ModelControl

    void print_logs( const int nlines ) const override;
    bool check_model_success( std::string_view ifsstat_path ) const override;


    // Delete copy constructor and assignment operator

    TestControl( const TestControl& ) = delete;
    TestControl& operator=( const TestControl& ) = delete;


  private:
    // Private member variables

    // Key model output logs
    std::vector<std::string> log_files = { "NODE.001_01", "ifs.stat", "rcf", "waminfo" };
};
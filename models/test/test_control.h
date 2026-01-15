//
// Test model control class header.
//  Glenn Carver, CPDN, 2025.

#pragma once

#include <string>
#include <filesystem>
#include <string_view>

#include "../../api/model_control.h"


namespace fs = std::filesystem;


class TestControl : public ModelControl { 

public:
    // Constructor and destructor methods
    TestControl(std::string_view vendor, std::string_view model, std::string_view version, std::string_view input_file) :
                ModelControl(vendor, model, version, input_file) {}
    ~TestControl() override = default;

    // Public interface methods
    // (overrides of pure virtual functions in ModelControl)
    //bool parse_command_line(int argc, char* argv[]) override;   // not yet implemented
    //bool setup() override;                                      // not yet implemented
    //bool set_envs() override;                            // not yet implemented 
    //int start() override;                                    // not yet implemented 
    //void do_step_tasks(int current_step) override;           // not yet implemented
    //bool teardown() override;                         // not yet implemented

    // Delete copy constructor and assignment operator
    
    TestControl(const TestControl&) = delete;
    TestControl& operator=(const TestControl&) = delete;
};
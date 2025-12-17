//
// Test model control class header.
//  Glenn Carver, CPDN, 2025.

#pragma once

#include <string>
#include <filesystem>

#include "../../api/model_control.h"


namespace fs = std::filesystem;


class TestControl : public ModelControl { 

public:
    TestControl() = default;
    virtual ~TestControl() = default;

    // Public interface methods
    // (overrides of pure virtual functions in ModelControl)
    bool parse_command_line(int argc, char* argv[]) override;
    bool setup() override;
    bool set_envs() override;
    int start() override;
    void do_step_tasks(int current_step) override;
    bool teardown() override;

    std::string get_vendor_name() const override;
    std::string get_model_name() const override;
    std::string get_model_version() const override;
    fs::path get_parameter_input_file() const override;

    void set_vendor_name(const std::string& vendor) override;
    void set_model_name(const std::string& model) override;
    void set_model_version(const std::string& version) override;
    void set_parameter_input_file(const fs::path& input_file) override;

    // Delete copy constructor and assignment operator
    
    TestControl(const TestControl&) = delete;
    TestControl& operator=(const TestControl&) = delete;
};
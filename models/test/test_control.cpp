//
// Implementation of the test model control class.
//  Glenn Carver, CPDN, 2025.

#include "test_control.h"

bool TestControl::parse_command_line(int argc, char* argv[]) {
    // TODO. Implement command line parsing for test model.
    return true;
}

bool TestControl::setup() {
    // TODO. Implement setup for test model.
    return true;
}

bool TestControl::set_envs() {
    // TODO. Implement environment variable setup for test model.
    return true;
}

int TestControl::start() {
    // TODO. Implement model start for test model.
    return 0;
}

void TestControl::do_step_tasks(int current_step) { 
    // TODO. Implement step tasks for test model.
}

bool TestControl::teardown() {
    // TODO. Implement teardown for test model.
    return true;
} 
std::string TestControl::get_vendor_name() const {
    // TODO. Implement getter for vendor name.
    return "TestVendor";
}
std::string TestControl::get_model_name() const {
    // TODO. Implement getter for model name.
    return "TestModel";
}

std::string TestControl::get_model_version() const {
    // TODO. Implement getter for model version.
    return "1.0";
}

fs::path TestControl::get_parameter_input_file() const {
    // TODO. Implement getter for parameter input file.
    return fs::path("test_input_file.txt");
}

void TestControl::set_vendor_name(const std::string& vendor) {
    // TODO. Implement setter for vendor name.
}

void TestControl::set_model_name(const std::string& model) {
    // TODO. Implement setter for model name.
}

void TestControl::set_model_version(const std::string& version) {
    // TODO. Implement setter for model version.
}

void TestControl::set_parameter_input_file(const fs::path& input_file) {
    // TODO. Implement setter for parameter input file.
}
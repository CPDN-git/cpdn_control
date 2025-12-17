//
//   Model control base class header.
//   This class is mainly virtual and provides an interface
//   for derived model control classes.
//
//   Glenn Carver, CPDN, 2025.

#pragma once

#include <string>
#include <filesystem>

namespace fs = std::filesystem;


class ModelControl {

public:
    ModelControl() = default;
    virtual ~ModelControl() = default;


    // Public interface methods
    // Pure virtual functions. Overrides must be provided in derived classes.

    // Parse command line arguments (may not need this if controller process handles it).
    virtual bool parse_command_line(int argc, char* argv[]) = 0;

    // Wrapper for various setup and initialization tasks before starting the model.
    virtual bool setup() = 0;

    // Set model environment variables in forked process to run the model task.
    virtual bool set_envs() = 0;

    // Start the model job
    virtual int start() = 0;

    // Function for handling tasks during the model run.
    // This should be called each step but may not do anything at every step.
    virtual void do_step_tasks(int current_step) = 0;

    // Tidy up and finalise after the model run has completed.
    virtual bool teardown() = 0;

    // Getters & setters for model information (placeholders)
    virtual std::string get_vendor_name() const = 0;
    virtual std::string get_model_name() const = 0;
    virtual std::string get_model_version() const = 0;
    virtual fs::path get_parameter_input_file() const = 0;

    virtual void set_vendor_name(const std::string& vendor) = 0;
    virtual void set_model_name(const std::string& model) = 0;
    virtual void set_model_version(const std::string& version) = 0;
    virtual void set_parameter_input_file(const fs::path& input_file) = 0;


    // Delete copy constructor and assignment operator
    // as these are not appropriate for this class.
    // NOTE: In a polymorphic base class, the copy constructor and assignment operator
    // should generally be deleted to prevent slicing and unintended behavior.
    ModelControl(const ModelControl&) = delete;
    ModelControl& operator=(const ModelControl&) = delete;

    // Delete move constructor and assignment operator
    ModelControl(ModelControl&&) = delete;
    ModelControl& operator=(ModelControl&&) = delete;

private:
    // Private member variables
    // Relates to the model XML input file read by the controller.

    std::string vendor_name;        // e.g. "ECMWF"
    std::string model_name;         // e.g. "OpenIFS"
    std::string model_version;      // e.g. "43r3"

    fs::path parameter_input_file;  // Usually this will be a fortran namelist file. e.g. "fort.4" for OpenIFS.
                                    // It is NOT intended for input data.

};
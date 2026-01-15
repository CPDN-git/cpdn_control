//
//   Model control base class header.
//   This class is mainly virtual and provides an interface
//   for derived model control classes.
//
//   Glenn Carver, CPDN, 2025.

#pragma once

#include <string>
#include <filesystem>
#include <string_view>

namespace fs = std::filesystem;


class ModelControl {

public:
    // Constructor and destructor methods
    // In the final version of this base class constructor, we'll probably use a one which takes the XML model config file as arg.
    // For now, use the default constructor and override it in derived classes.
    // C++ note: Compiler will not generate a default constructor if any other constructors are defined in the derived class.
    // C++ note: Destructor should be virtual in base classes so deleting derived class objects via base class pointers works correctly.
    ModelControl() = default;
    virtual ~ModelControl() = default;


    // Public interface methods (examples for now -- will change as more implmentation is done through the class)
    // Pure virtual functions. Overrides must be provided in derived classes.

    // Prints last n lines of key log files produced by the model.
    virtual void print_logs(const int nlines) const = 0;

    // Parse command line arguments (may not need this if controller process handles it).
    //virtual bool parse_command_line(int argc, char* argv[]) = 0;

    // Wrapper for various setup and initialization tasks before starting the model.
    //virtual bool setup() = 0;

    // Set model environment variables in forked process to run the model task.
    //virtual bool set_envs() = 0;

    // Start the model job
    //virtual int start() = 0;

    // Function for handling tasks during the model run.
    // This should be called each step but may not do anything at every step.
    //virtual void do_step_tasks(int current_step) = 0;

    // Tidy up and finalise after the model run has completed.
    //virtual bool teardown() = 0;

    // Getters & setters for model information (placeholders)
    // C++ note: provide "default implementation unless overridden" so must still be virtual.
    virtual std::string get_vendor_name() const { return vendor_name; }
    virtual std::string get_model_name() const  { return model_name; }
    virtual std::string get_model_version() const { return model_version; }
    virtual fs::path get_parameter_input_file() const { return parameter_input_file; }


    // Delete copy constructor and assignment operator as these are not appropriate for this class.
    // C++ note: In a polymorphic base class, the copy constructor and assignment operator
    // should generally be deleted to prevent slicing and unintended behavior.
    ModelControl(const ModelControl&) = delete;
    ModelControl& operator=(const ModelControl&) = delete;

    // Delete move constructor and assignment operator
    ModelControl(ModelControl&&) = delete;
    ModelControl& operator=(ModelControl&&) = delete;


protected:
    // Protected constructor for use by derived classes (may change in future when we use the model XML config file)
    // Use init list here, no need to use 'setters' in constructor.
    // C++ note. Allows keeping member variables private while still enabling derived classes to use init-list construction.
    ModelControl(std::string_view vendor, std::string_view model, std::string_view version, const fs::path& input_file)
            :  vendor_name(vendor),
               model_name(model),
               model_version(version),
               parameter_input_file(input_file) {};

    // Setters for model information (protected so only accessible to derived classes)

    void set_vendor_name(std::string_view vendor) { vendor_name = vendor; }
    void set_model_name(std::string_view model) { model_name = model; }
    void set_model_version(std::string_view version) { model_version = version; }
    void set_parameter_input_file(const fs::path& input_file) { parameter_input_file = input_file; }


private:
    // Private member variables (not visible to derived classes; derived classes should use getters/setters)
    // Relates to the model XML input file read by the controller.

    std::string vendor_name;        // e.g. "ECMWF"
    std::string model_name;         // e.g. "OpenIFS"
    std::string model_version;      // e.g. "43r3"

    fs::path parameter_input_file;  // Usually this will be a fortran namelist file. e.g. "fort.4" for OpenIFS.
                                    // It is NOT intended for input data.

};
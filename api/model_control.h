//
//   Model control base class header.
//   This class is mainly abstract and provides an interface API
//   for derived model control classes.
//
//   Glenn Carver, CPDN, 2025-.

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "model_input_manifest.h"

namespace fs = std::filesystem;


/**
 * @brief Struct to hold key time variables read from the model control input (e.g. namelist files)
 * 
 * Notes:
 *  - Assume any model can be monitored using a fixed timestep with a forecast time in number of steps.
 *  - Assume restart and model output frequencies are also a fixed interval.
 *  - For nested models, assume cpdn_control only monitors the top level model.
 *  - This should cover most models.
 * 
 *  Any model with a variable timestepping scheme or variable output frequency would need to be
 * handled in a model-specific way, which is outside the scope of this generic controller. It would
 * likely require coding adding callback functions to the model instance class.
 * 
 */
struct ModelControlInputData {
    bool ok = false;
    fs::path source_file;
    std::string error_step;    // TODO: revise.  I don't like GPT putting the error vars in with the control data
    std::string error_field;
    std::string error_message;

    int timestep_seconds = 0;    // Model timestep in seconds e.g. OpenIFS : UTSTEP
    int output_interval = 0;     // Interval between creation of new output file in model steps e.g. OpenIFS : NFRPOS
    int restart_interval = 0;    // Output of model restarts in model steps e.g. OpenIFS : NFRRES raw value: +ve model steps, -ve hours
    int total_steps = 0;         // Length of forecast in model steps e.g. OpenIFS : CUSTOP
    double forecast_length_time = 0.0;
};

// 'inline' here avoids multiple definition errors when this header is included in multiple translation units.
inline ModelControlInputData make_parse_error( const fs::path& source_file, std::string_view error_step, std::string_view error_field,
                                               std::string_view message )
{
    ModelControlInputData result;
    result.source_file = source_file;
    result.error_step = std::string( error_step );
    result.error_field = std::string( error_field );
    result.error_message = std::string( message );
    return result;
}


class ModelControl {

  public:
    // Constructor and destructor methods

    // Use the default constructor and override it in derived classes.
    // C++ note: Compiler will not generate a default constructor if any other constructors are defined in the derived class.
    // C++ note: Destructor should be virtual in base classes so deleting derived class objects via base class pointers works correctly.
    ModelControl() = default;
    virtual ~ModelControl() = default;


    // Public interface methods

    // Pure virtual functions. Overrides *must* be provided in derived classes.

    // Checks the model has completed successfully.
    virtual bool check_model_success() const = 0;

    // Prints last n lines of key log files produced by the model.
    virtual void print_logs( const int nlines ) const = 0;

    // Returns true if a valid model restart file exists.
    virtual bool restart_exists() const = 0;

    // Parse model restart metadata so the controller knows which step the model is restarting from.
    virtual bool parse_restart( std::string& step ) const = 0;


    // Virtual functions with default implementations. Overrides may be provided in derived classes, but are not required.

    // Wrapper for various setup and initialization tasks before starting the model.
    // This should be called after the model files have been staged (unpacked)
    // but before the model is started. It could be used for example to modify
    // the model namelist if restarting (as WRF needs).
    virtual bool setup( const fs::path& slot_path ) const { return true; }

    // Wrapper for handling tasks during the model run.
    // This should be called on a model step but may not do anything at every step.
    // Example use would be to run external diagnostics at set intervals, or
    // prune restart files as in the case of WRF.
    // IMPORTANT. Because of the asynchronous nature of the control code tracking model steps,
    // we cannot guarantee it's called every step.
    virtual bool do_step_tasks( int current_step, const fs::path& slot_path ) { return true; }

    // Tidy up and finalize after the model run has completed.
    virtual bool finalize( const fs::path& slot_path ) const { return true; }


    // Getters & setters for model information (placeholders)

    // Pure functions first to enforce overrides in derived classes,
    // then virtual functions with default implementations.

    // Get list of model input files to unpack from the project directory.
    virtual ModelInputManifest get_input_manifest( const std::string& wu_id ) const = 0;

    // Read the main model input control file (e.g. namelist) and extract key params.
    virtual ModelControlInputData parse_control_input() const = 0;

    // Determine the current model step count; return true if successful, false otherwise.
    virtual bool get_current_step( int& current_step, const int total_steps ) const = 0;

    // Provide the nominal model output filenames associated with a particular model step.
    // This is useful for model-internal step-based filename generation, but controller code
    // should not assume these files are necessarily complete or safe to copy yet.
    virtual std::vector<std::string> get_output_filenames( int step ) const = 0;

    // Provide all model output filenames that are currently safe for the controller to copy
    // out of the slot directory as of the current model step.
    // This is the controller-facing seam for output collection; it is model-owned because
    // readiness and file granularity differ between models (e.g. one step per file vs many).
    virtual std::vector<std::string> get_copyable_output_filenames( int current_step ) const = 0;

    // Check whether a filename is a model output file that should be considered for upload packaging.
    virtual bool is_output_filename( std::string_view filename ) const = 0;

    // Check whether a filename is a model restart artifact for this model.
    virtual bool is_restart_filename( std::string_view filename ) const = 0;

    // Responsible for setting up any model input directories and/or symlinks as needed before staging the input files.
    virtual bool setup_directories( const fs::path& slot_path ) const = 0;

    virtual std::vector<std::string> get_log_filenames() const = 0;

    // Gives the minimum and maximum number of threads the model can use based on the model configuration and/or system resources.
    virtual void get_nthreads_range( int& min_threads, int& max_threads ) const
    {
        min_threads = 1;
        max_threads = 1;
    }

    virtual std::string get_vendor_name() const { return vendor_name; }
    virtual std::string get_model_name() const { return model_name; }
    virtual std::string get_model_version() const { return model_version; }
    virtual std::string get_executable_name() const { return executable; }
    virtual std::vector<std::string> get_env_vars( const std::string& slot_path, const std::string& nthreads, std::string& err_msg ) const
    {
        (void)slot_path;
        (void)nthreads;
        err_msg.clear();
        return {};
    }


    // Delete copy constructor and assignment operator as these are not appropriate for this class.
    // C++ note: In a polymorphic base class, the copy constructor and assignment operator
    // should generally be deleted to prevent slicing and unintended behavior.
    ModelControl( const ModelControl& ) = delete;
    ModelControl& operator=( const ModelControl& ) = delete;

    // Delete move constructor and assignment operator
    ModelControl( ModelControl&& ) = delete;
    ModelControl& operator=( ModelControl&& ) = delete;


  protected:
    // Protected constructor for use by derived classes *only*.
    // Use init list here, no need to use 'setters' in constructor.
    // C++ note. Allows keeping member variables private while still enabling derived classes to use init-list construction.
    ModelControl( std::string_view vendor, std::string_view model, std::string_view version, std::string_view exe )
        : vendor_name( vendor ), model_name( model ), model_version( version ), executable( exe ) {};

    // Protected member variables (accessible to derived classes, immutable after construction)
    // C++ note. Members are initialized in declaration order, not initializer-list order.
    // Best practice: keep the initializer list in the same order as member declarations to avoid confusion.

    const std::string vendor_name;      // e.g. "ECMWF"
    const std::string model_name;       // e.g. "OpenIFS"
    const std::string model_version;    // e.g. "43r3"
    const std::string executable;       // e.g. "oifs_43r3_model.exe", "oifs_43r3_omp_model.exe", "test_model"
};

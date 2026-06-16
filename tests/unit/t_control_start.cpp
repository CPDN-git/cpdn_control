// Test startup/restart bootstrap seam
//
//  Glenn Carver, CPDN, 2026

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "../../src/control_start.h"
#include "unit_tests.h"

namespace fs = std::filesystem;

namespace {

class FakeModelControl : public ModelControl {

  public:
    FakeModelControl() : ModelControl( "CPDN", "fake_model", "1.0", "fake_exe" ) {}

    void print_logs( const int ) const override {}
    bool check_model_success() const override { return true; }
    bool restart_exists() const override { return restart_file_exists; }
    bool restart_ctl_read( std::string& step, std::string& time ) const override
    {
        step = restart_step;
        time = restart_time;
        return restart_read_ok;
    }
    ModelInputManifest get_input_manifest( const std::string& ) const override { return {}; }
    ModelControlInputData parse_control_input() const override { return {}; }
    bool get_current_step( int&, const int ) const override { return false; }
    std::vector<std::string> get_output_filenames( int ) const override { return {}; }
    bool is_output_filename( std::string_view ) const override { return false; }
    bool is_restart_filename( std::string_view ) const override { return false; }
    std::vector<std::string> get_log_filenames() const override { return {}; }
    bool setup_directories( const fs::path& ) const override { return true; }

    bool restart_file_exists = false;
    bool restart_read_ok = true;
    std::string restart_step = "0";
    std::string restart_time = "00000000";
};


fs::path make_temp_dir()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path dir = "tmp_control_start_tests_" + std::to_string( now );
    std::error_code ec;
    fs::create_directory( dir, ec );
    return dir;
}


bool write_empty_file( const fs::path& path )
{
    std::ofstream out( path, std::ios::out | std::ios::trunc );
    return static_cast<bool>( out );
}


bool write_progress_file( const fs::path& dir, int last_completed_step, int upload_file_number = 1, double last_upload_time = 0.0 )
{
    ProgressFileHandler progress_file( dir.string() );
    TaskState task;
    task.current_cpu_time = 10.0;
    task.prior_acc_cpu_time = 5.0;
    task.upload_file_number = upload_file_number;
    task.last_completed_step = last_completed_step;
    task.last_upload_time = last_upload_time;
    task.model_completed = 0;
    std::string err_msg;
    return progress_file.write( task, err_msg );
}

}    // namespace


int t_control_start()
{
    TEST( "t_control_start" );

    const int restart_interval_steps = 12;
    std::string err_msg;

    std::cout << "Subtest: fresh run with no progress file and no restart file\n";
    {
        const fs::path dir = make_temp_dir();
        FakeModelControl model_ctrl;
        ProgressFileHandler progress_file( dir.string() );
        TaskState tstate;

        auto result = initialize_task_state_from_restart( model_ctrl, progress_file, restart_interval_steps, tstate, err_msg );
        if ( !result.ok || result.startup_mode != TaskStartupMode::fresh_run || result.log_message.empty() || !err_msg.empty() ) {
            TEST_FAIL;
            std::cout << "Unexpected fresh-run result\n";
            return EXIT_FAILURE;
        }
        std::error_code ec;
        fs::remove_all( dir, ec );
    }

    std::cout << "Subtest: valid restart with progress file and restart file\n";
    {
        const fs::path dir = make_temp_dir();
        FakeModelControl model_ctrl;
        model_ctrl.restart_file_exists = true;
        model_ctrl.restart_step = "13";
        ProgressFileHandler progress_file( dir.string() );
        TaskState tstate;

        if ( !write_progress_file( dir, 13 ) ) {
            TEST_FAIL;
            std::cout << "Unable to write valid progress file\n";
            return EXIT_FAILURE;
        }

        auto result = initialize_task_state_from_restart( model_ctrl, progress_file, restart_interval_steps, tstate, err_msg );
        if ( !result.ok || result.startup_mode != TaskStartupMode::restart_run ||
             result.log_message.find( "Model is restarting" ) == std::string::npos || tstate.last_completed_step != 13 ) {
            TEST_FAIL;
            std::cout << "Unexpected restart result or adjusted step: " << tstate.last_completed_step << "\n";
            return EXIT_FAILURE;
        }
        std::error_code ec;
        fs::remove_all( dir, ec );
    }

    std::cout << "Subtest: invalid restart when restart step exceeds progress step\n";
    {
        const fs::path dir = make_temp_dir();
        FakeModelControl model_ctrl;
        model_ctrl.restart_file_exists = true;
        model_ctrl.restart_step = "14";
        ProgressFileHandler progress_file( dir.string() );
        TaskState tstate;

        if ( !write_progress_file( dir, 13 ) ) {
            TEST_FAIL;
            std::cout << "Unable to write valid progress file\n";
            return EXIT_FAILURE;
        }

        auto result = initialize_task_state_from_restart( model_ctrl, progress_file, restart_interval_steps, tstate, err_msg );
        if ( result.ok || result.startup_mode != TaskStartupMode::invalid ||
             err_msg.find( "restart greater than last_completed_step" ) == std::string::npos ) {
            TEST_FAIL;
            std::cout << "Unexpected invalid-restart result: " << err_msg << "\n";
            return EXIT_FAILURE;
        }
        std::error_code ec;
        fs::remove_all( dir, ec );
    }

    std::cout << "Subtest: empty progress file is invalid and requests model logs\n";
    {
        const fs::path dir = make_temp_dir();
        FakeModelControl model_ctrl;
        ProgressFileHandler progress_file( dir.string() );
        TaskState tstate;

        if ( !write_empty_file( dir / std::string( progressfile_name ) ) ) {
            TEST_FAIL;
            std::cout << "Unable to write empty progress file\n";
            return EXIT_FAILURE;
        }

        auto result = initialize_task_state_from_restart( model_ctrl, progress_file, restart_interval_steps, tstate, err_msg );
        if ( result.ok || !result.print_model_logs || err_msg.find( "progress file exists, but is empty" ) == std::string::npos ) {
            TEST_FAIL;
            std::cout << "Unexpected empty-progress result: " << err_msg << "\n";
            return EXIT_FAILURE;
        }
        std::error_code ec;
        fs::remove_all( dir, ec );
    }

    std::cout << "Subtest: progress file without restart file below restart threshold allows restart from beginning\n";
    {
        const fs::path dir = make_temp_dir();
        FakeModelControl model_ctrl;
        ProgressFileHandler progress_file( dir.string() );
        TaskState tstate;

        if ( !write_progress_file( dir, 5 ) ) {
            TEST_FAIL;
            std::cout << "Unable to write valid progress file\n";
            return EXIT_FAILURE;
        }

        auto result = initialize_task_state_from_restart( model_ctrl, progress_file, restart_interval_steps, tstate, err_msg );
        if ( !result.ok || result.startup_mode != TaskStartupMode::fresh_run || !err_msg.empty() ) {
            TEST_FAIL;
            std::cout << "Unexpected progress-without-rcf result: " << err_msg << "\n";
            return EXIT_FAILURE;
        }
        std::error_code ec;
        fs::remove_all( dir, ec );
    }

    std::cout << "Subtest: progress file without restart file at restart threshold is invalid\n";
    {
        const fs::path dir = make_temp_dir();
        FakeModelControl model_ctrl;
        ProgressFileHandler progress_file( dir.string() );
        TaskState tstate;

        if ( !write_progress_file( dir, 12 ) ) {
            TEST_FAIL;
            std::cout << "Unable to write valid progress file\n";
            return EXIT_FAILURE;
        }

        auto result = initialize_task_state_from_restart( model_ctrl, progress_file, restart_interval_steps, tstate, err_msg );
        if ( result.ok || !result.print_model_logs || err_msg.find( "progress file exists, but rcf file does not exist" ) == std::string::npos ) {
            TEST_FAIL;
            std::cout << "Unexpected invalid mixed-state result: " << err_msg << "\n";
            return EXIT_FAILURE;
        }
        std::error_code ec;
        fs::remove_all( dir, ec );
    }

    std::cout << "Subtest: restart file without progress file is invalid\n";
    {
        const fs::path dir = make_temp_dir();
        FakeModelControl model_ctrl;
        model_ctrl.restart_file_exists = true;
        ProgressFileHandler progress_file( dir.string() );
        TaskState tstate;

        auto result = initialize_task_state_from_restart( model_ctrl, progress_file, restart_interval_steps, tstate, err_msg );
        if ( result.ok || !result.print_model_logs || err_msg.find( "rcf file exists, but progress file does not exist" ) == std::string::npos ) {
            TEST_FAIL;
            std::cout << "Unexpected missing-progress result: " << err_msg << "\n";
            return EXIT_FAILURE;
        }
        std::error_code ec;
        fs::remove_all( dir, ec );
    }

    TEST_SUCCESS;
    return EXIT_SUCCESS;
}

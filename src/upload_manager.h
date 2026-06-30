#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "cpdn_control.h"

class ModelControl;

struct UploadOperationResult {
    bool ok = true;
    int finish_code = 1;
};

class UploadManager {

  public:
    UploadManager( const BoincConfig& bconfig, const ModelControl& model_ctrl, std::filesystem::path upload_dir, std::string result_base_name,
                   int total_steps, int upload_interval )
        : bconfig_( bconfig ), model_ctrl_( model_ctrl ), upload_dir_( std::move( upload_dir ) ), result_base_name_( std::move( result_base_name ) ),
          total_steps_( total_steps ), upload_interval_( upload_interval )
    {
    }

    bool uploads_enabled() const { return upload_interval_ > 0; }
    bool standalone() const { return bconfig_.standalone; }
    const std::filesystem::path& upload_dir() const { return upload_dir_; }

    void move_copyable_output_files( int step ) const;
    UploadOperationResult process_scheduled_upload( BoincRuntime& runtime, TaskState& tstate, int observed_step ) const;
    UploadOperationResult finalize_remaining_uploads( BoincRuntime& runtime, TaskState& tstate, int final_step, bool include_log_files,
                                                      bool allow_boinc_child_control = true ) const;
    void cleanup_upload_dir() const;

  private:
    struct UploadSendResult {
        bool ok = true;
        bool archive_created = false;
        bool upload_attempted = false;
        std::filesystem::path archive_path;
        std::string logical_upload_name;
        std::string error_step;
        int error_code = 0;
        int finish_code = 1;
        std::string error_message;
    };

    UploadOperationResult collect_upload_output_files( std::vector<std::filesystem::path>& out ) const;
    UploadOperationResult ensure_nonempty_upload_payload( int upload_file_number, int current_step, std::string_view placeholder_reason,
                                                          std::vector<std::filesystem::path>& files_to_zip ) const;
    void add_final_log_files( std::vector<std::filesystem::path>& files_to_zip ) const;
    UploadSendResult zip_and_send_upload( BoincRuntime& runtime, TaskState& tstate, int upload_file_number,
                                          const std::vector<std::filesystem::path>& files_to_zip, bool allow_boinc_child_control ) const;
    static void report_upload_send_failure( std::string_view context, const UploadSendResult& result );

    const BoincConfig& bconfig_;
    const ModelControl& model_ctrl_;
    std::filesystem::path upload_dir_;
    std::string result_base_name_;
    int total_steps_ = 0;
    int upload_interval_ = 0;
};

#include "upload_manager.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

#include "api/model_control.h"
#include "boinc/boinc_api.h"
#include "boinc/error_numbers.h"
#include "lib/utils.h"

namespace fs = std::filesystem;

namespace {

void log_boinc_api_error( const char* api_name, const int retval )
{
    std::cerr << ".." << api_name << " failed (" << retval << "): " << boincerror( retval ) << '\n';
}

}    // namespace


void UploadManager::move_copyable_output_files( const int step ) const
{
    auto copyable_output_files = model_ctrl_.get_copyable_output_filenames( step );
    for ( const auto& result : copyable_output_files ) {
        const int retval = move_result_file( bconfig_.slot_path, upload_dir_, result );
        if ( retval != 0 ) {
            std::cerr << "..Copying " << result
                      << " model output file to the temp upload folder in projects directory failed; continuing with remaining files\n";
        }
    }
}


UploadOperationResult UploadManager::collect_upload_output_files( std::vector<fs::path>& out ) const
{
    std::error_code ec;

    for ( const auto& entry : fs::directory_iterator( upload_dir_, ec ) ) {
        if ( ec ) {
            std::cerr << "..Unable to scan upload directory: " << upload_dir_ << " (" << ec.message() << ")\n";
            return { false, ec.value() };
        }
        if ( !entry.is_regular_file( ec ) ) {
            if ( ec ) {
                std::cerr << "..Unable to read directory entry: " << entry.path() << " (" << ec.message() << ")\n";
                return { false, ec.value() };
            }
            continue;
        }

        const auto filename = entry.path().filename().string();
        if ( model_ctrl_.is_output_filename( filename ) ) {
            out.push_back( entry.path() );
            std::cerr << "Adding to the zip: " << entry.path().string() << '\n';
        }
    }

    return {};
}


UploadOperationResult UploadManager::ensure_nonempty_upload_payload( const int upload_file_number, const int current_step,
                                                                     const std::string_view placeholder_reason,
                                                                     std::vector<fs::path>& files_to_zip ) const
{
    if ( !files_to_zip.empty() ) {
        return {};
    }

    fs::path placeholder_path;
    std::string error_msg;
    if ( !create_upload_placeholder_file( upload_dir_, upload_file_number, current_step, total_steps_, placeholder_reason, placeholder_path,
                                          &error_msg ) ) {
        std::cerr << "..Failed to create upload placeholder file: " << error_msg << '\n';
        return { false, 1 };
    }

    std::cerr << "Adding upload placeholder file to archive: " << placeholder_path << '\n';
    files_to_zip.push_back( placeholder_path );
    return {};
}


void UploadManager::add_final_log_files( std::vector<fs::path>& files_to_zip ) const
{
    for ( const auto& logfile : model_ctrl_.get_log_filenames() ) {
        fs::path logpath = bconfig_.slot_path;
        logpath /= logfile;
        if ( fs::exists( logpath ) ) {
            files_to_zip.push_back( logpath.string() );
            std::cerr << "Adding model log file to the upload zipfile: " << logpath << '\n';
        }
    }
}


void UploadManager::report_upload_send_failure( const std::string_view context, const UploadSendResult& result )
{
    std::cerr << "..Failed to send " << context;
    if ( !result.archive_path.empty() ) {
        std::cerr << " archive '" << result.archive_path << "'";
    }
    if ( !result.logical_upload_name.empty() ) {
        std::cerr << " as logical file '" << result.logical_upload_name << "'";
    }
    if ( !result.error_step.empty() ) {
        std::cerr << " at step '" << result.error_step << "'";
    }
    if ( result.error_code != 0 ) {
        std::cerr << " (code " << result.error_code << ")";
    }
    if ( !result.error_message.empty() ) {
        std::cerr << ": " << result.error_message;
    }
    std::cerr << '\n';
}


UploadManager::UploadSendResult UploadManager::zip_and_send_upload( BoincRuntime& runtime, TaskState& tstate, const int upload_file_number,
                                                                    const std::vector<fs::path>& files_to_zip,
                                                                    const bool allow_boinc_child_control ) const
{
    UploadSendResult result;
    result.archive_path = fs::path( bconfig_.project_dir ) / ( result_base_name_ + "_" + std::to_string( upload_file_number ) + ".zip" );
    result.logical_upload_name = "upload_file_" + std::to_string( upload_file_number ) + ".zip";

    if ( files_to_zip.empty() ) {
        return result;
    }

    std::cerr << "Compressing upload file: " << result.archive_path << '\n';
    const int zip_ret = zip_and_delete( result.archive_path.string(), files_to_zip );
    if ( zip_ret != 0 ) {
        result.ok = false;
        result.error_step = "zip";
        result.error_code = zip_ret;
        result.error_message = "failed to create upload archive";
        return result;
    }
    result.archive_created = true;

    if ( bconfig_.standalone ) {
        return result;
    }

    result.upload_attempted = true;

    std::cerr << "Waiting for file operations to complete...(20 secs)" << std::endl;
    if ( !sleep_with_boinc_poll( runtime, bconfig_.standalone, 20 ) ) {
        if ( allow_boinc_child_control ) {
            if ( !apply_boinc_suspend_resume( tstate.child_process, runtime ) ) {
                result.ok = false;
                result.error_step = "boinc_poll";
                result.error_message = "BOINC status changed before upload could be submitted";
                result.finish_code = runtime.client_status.quit_request ? 0 : 1;
                return result;
            }
        } else {
            while ( runtime.client_status.suspended ) {
                sleep_seconds( 1 );
                boinc_get_status( &runtime.client_status );
            }
            if ( runtime.client_status.quit_request || runtime.client_status.abort_request || runtime.client_status.no_heartbeat ) {
                result.ok = false;
                result.error_step = "boinc_poll";
                result.error_message = "BOINC status changed before upload could be submitted";
                result.finish_code = runtime.client_status.quit_request ? 0 : 1;
                return result;
            }
        }
    }

    std::string upload_name = result.logical_upload_name;
    const int upload_ret = boinc_upload_file( upload_name );
    if ( upload_ret != 0 ) {
        log_boinc_api_error( "boinc_upload_file", upload_ret );
        result.ok = false;
        result.error_step = "boinc_upload_file";
        result.error_code = upload_ret;
        result.error_message = boincerror( upload_ret );
        return result;
    }

    // BOINC uploads are asynchronous and the client owns transfer retries. In
    // particular, a CPDN upload may remain pending while the upload server is
    // unavailable. boinc_upload_status() is therefore not useful here: an
    // immediate query can return ERR_NOT_FOUND before the client has published
    // a status, while waiting for completion could block the model indefinitely.
    // A successful boinc_upload_file() call means the request was handed off.

    return result;
}


UploadOperationResult UploadManager::process_scheduled_upload( BoincRuntime& runtime, TaskState& tstate, const int observed_step ) const
{
    if ( !uploads_enabled() || ( observed_step - tstate.last_upload_step ) < upload_interval_ || observed_step >= total_steps_ ) {
        return {};
    }

    std::vector<fs::path> files_to_zip;

    std::cerr << "Model result upload step reached. Starting a new upload: " << " current step: " << observed_step
              << ", last upload step: " << tstate.last_upload_step << ", upload interval: " << upload_interval_ << ", total steps: " << total_steps_
              << std::endl;

    boinc_begin_critical_section();

    auto collect_result = collect_upload_output_files( files_to_zip );
    if ( !collect_result.ok ) {
        std::cerr << "Adding model output files to the upload zip failed!\n";
        boinc_end_critical_section();
        return collect_result;
    }

    auto payload_result = ensure_nonempty_upload_payload( tstate.upload_file_number, observed_step,
                                                          "scheduled upload interval reached with no model output files ready", files_to_zip );
    if ( !payload_result.ok ) {
        boinc_end_critical_section();
        return payload_result;
    }

    const std::string upload_file_name = "upload_file_" + std::to_string( tstate.upload_file_number ) + ".zip";
    std::cerr << "Uploading the results file: " << upload_file_name << '\n';

    auto upload_result = zip_and_send_upload( runtime, tstate, tstate.upload_file_number, files_to_zip, true );
    if ( !upload_result.ok ) {
        report_upload_send_failure( "result upload", upload_result );
        boinc_end_critical_section();
        return { false, upload_result.finish_code };
    }
    if ( upload_result.upload_attempted ) {
        std::cerr << "Submitted upload to BOINC: " << upload_file_name << '\n';
    }

    tstate.last_upload_step = observed_step;
    tstate.upload_file_number++;
    boinc_end_critical_section();
    return {};
}


UploadOperationResult UploadManager::finalize_remaining_uploads( BoincRuntime& runtime, TaskState& tstate, const int final_step,
                                                                 const bool include_log_files, const bool allow_boinc_child_control ) const
{
    if ( !uploads_enabled() ) {
        return {};
    }

    std::vector<fs::path> files_to_zip;
    boinc_begin_critical_section();

    move_copyable_output_files( final_step );

    if ( include_log_files ) {
        add_final_log_files( files_to_zip );
    }

    auto collect_result = collect_upload_output_files( files_to_zip );
    if ( !collect_result.ok ) {
        std::cerr << "Adding model output files to the upload zip failed!\n";
    }

    const int starting_upload_index = tstate.upload_file_number;
    const int total_upload_count = std::max( expected_upload_file_count( total_steps_, upload_interval_ ), starting_upload_index + 1 );

    for ( int upload_index = starting_upload_index; upload_index < total_upload_count; ++upload_index ) {
        auto upload_files = upload_index == starting_upload_index ? files_to_zip : std::vector<fs::path>{};
        const std::string_view placeholder_reason = upload_index == starting_upload_index
                                                        ? "final upload payload was empty; sending placeholder to satisfy BOINC upload contract"
                                                        : "task ended before reaching this scheduled upload interval";

        auto payload_result = ensure_nonempty_upload_payload( upload_index, final_step, placeholder_reason, upload_files );
        if ( !payload_result.ok ) {
            boinc_end_critical_section();
            return payload_result;
        }

        const std::string upload_file_name = "upload_file_" + std::to_string( upload_index ) + ".zip";
        if ( !bconfig_.standalone ) {
            std::cerr << "Uploading the final file: " << upload_file_name << '\n';
        }

        auto upload_result = zip_and_send_upload( runtime, tstate, upload_index, upload_files, allow_boinc_child_control );
        if ( !upload_result.ok ) {
            report_upload_send_failure( "final upload", upload_result );
            boinc_end_critical_section();
            return { false, upload_result.finish_code };
        }
        if ( upload_result.upload_attempted ) {
            std::cerr << "Submitted final upload to BOINC: " << upload_file_name << '\n';
        }
        tstate.upload_file_number = upload_index + 1;
    }

    boinc_end_critical_section();
    return {};
}


bool UploadManager::cleanup_upload_dir() const
{
    std::error_code ec;
    fs::remove_all( upload_dir_, ec );
    if ( ec ) {
        std::cerr << "Failed to remove temporary upload directory '" << upload_dir_ << "': " << ec.message()
                  << ". The directory may need to be removed manually.\n";
        return false;
    }
    return true;
}

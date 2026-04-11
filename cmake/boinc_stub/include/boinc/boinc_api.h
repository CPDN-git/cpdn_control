#pragma once

#include <filesystem>
#include <string>

struct APP_INIT_DATA {
    int app_version = 100;
    const char* app_name = "";
    const char* project_dir = "";
    const char* boinc_dir = "";
    const char* wu_name = "";
    const char* result_name = "";
    double ncpus = 1.0;
};

struct BOINC_STATUS {
    int quit_request = 0;
    int abort_request = 0;
    int no_heartbeat = 0;
    int suspended = 0;
};

struct BOINC_OPTIONS {
    bool main_program = false;
    bool multi_process = false;
    bool check_heartbeat = false;
    bool handle_process_control = false;
    bool direct_process_action = false;
    bool send_status_msgs = false;
};

inline int boinc_init() { return 0; }

inline int boinc_parse_init_data_file() { return 0; }

inline void boinc_get_init_data( APP_INIT_DATA& data ) { data = APP_INIT_DATA{}; }

inline int boinc_is_standalone() { return 1; }

inline void boinc_options_defaults( BOINC_OPTIONS& options ) { options = BOINC_OPTIONS{}; }

inline int boinc_init_options( BOINC_OPTIONS* ) { return 0; }

inline void boinc_get_status( BOINC_STATUS* status )
{
    if ( status ) {
        *status = BOINC_STATUS{};
    }
}

inline int boinc_resolve_filename_s( const char* logical_name, std::string& resolved_name )
{
    resolved_name = logical_name ? logical_name : "";
    return 0;
}

inline int boinc_upload_file( const std::string& ) { return 0; }

inline int boinc_upload_status( const std::string& ) { return 0; }

inline void boinc_begin_critical_section() {}

inline void boinc_end_critical_section() {}

inline void boinc_finish( int ) {}

inline void boinc_report_app_status( double, double, double ) {}

inline void boinc_fraction_done( double ) {}

inline int boinc_send_trickle_up( const char*, const char* ) { return 0; }

inline int boinc_copy( const char* src, const char* dst )
{
    if ( src == nullptr || dst == nullptr ) {
        return 1;
    }

    std::error_code ec;
    std::filesystem::copy_file( src, dst, std::filesystem::copy_options::overwrite_existing, ec );
    return ec ? 1 : 0;
}

inline const char* boincerror( int ) { return "BOINC stub"; }

#pragma once

#include <string>
#include <vector>

struct TaskConfig;

struct ParseResult {

    std::string batch = "";       // CPDN assigned batch ID for this task
    std::string workunit = "";    // CPDN assigned workunit ID for this task
    std::string memberid = "";    // CPDN unique member ID (umid)
    std::string app_name = "";
    // Opaque middle component of the app-bundle logical filename. This is filename
    // metadata only; it is not model runtime configuration.
    std::string filename_label = "";
    int upload_interval = 0;    // Controller upload interval in model steps; 0 disables result uploads.

    int exit_code = 0;
    bool ok = true;
};

ParseResult parse_args( int argc, char** argv );
bool process_args( const ParseResult& parse_result, TaskConfig& tconfig, std::string& err_msg );

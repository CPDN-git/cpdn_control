#pragma once

#include <string>
#include <vector>

struct ParseResult {

    std::string batch = "";       // CPDN assigned batch ID for this task
    std::string workunit = "";    // CPDN assigned workunit ID for this task
    std::string memberid = "";    // CPDN unique member ID (umid)
    std::string app_name = "";
    std::string startdate = "";
    std::string fcast_len = "";    // Forecast length (units?). Although the model knows this we need it before starting the model for file names.
    int upload_interval = 0;
    std::vector<std::string> ancil_files = {};
    std::vector<std::string> model_args = {};    // Model-specific args passed through as strings in single argument.

    int exit_code = 0;
    bool ok = true;
};

ParseResult parse_args( int argc, char** argv );

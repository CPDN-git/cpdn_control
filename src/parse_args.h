#pragma once

#include <string>
#include <vector>

struct ParseResult {

    std::string batch = "";       // CPDN assigned batch ID for this task
    std::string workunit = "";    // CPDN assigned workunit ID for this task
    std::string memberid = "";    // CPDN unique member ID (umid)
    std::string app_name = "";
    std::string filename_startdate = "";
    std::string filename_fclen = "";    // Forecast-length token embedded in CPDN download filenames; not passed to the model.
    int upload_interval = 0;
    std::vector<std::string> ancil_files = {};
    std::vector<std::string> model_args = {};    // Model-specific args passed through as strings in single argument.

    int exit_code = 0;
    bool ok = true;
};

ParseResult parse_args( int argc, char** argv );

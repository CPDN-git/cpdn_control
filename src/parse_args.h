#pragma once

#include <string>
#include <vector>

struct ParsedArgs {
    std::string cpdn_batch = "";
    std::string cpdn_wu = "";
    std::string cpdn_app = "";
    int cpdn_upload_int = 0;
    std::vector<std::string> cpdn_ancil_files = {};
    std::vector<std::string> model_args = {};
};

struct ParseResult {
    ParsedArgs args;
    int exit_code = 0;
    bool ok = true;
};

ParseResult parse_args( int argc, char** argv );

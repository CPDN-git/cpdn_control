#include "parse_args.h"

#include "../tools/CLI11/CLI/CLI.hpp"

ParseResult parse_args( int argc, char** argv )
{
    ParseResult result;

    CLI::App app{ "CPDN control" };
    app.allow_extras();    // allow unknown options (e.g., --nthreads) coming from boinc users.

    auto* cpdn = app.add_option_group( "cpdn" );
    cpdn->add_option( "--cpdn_batch", result.args.cpdn_batch, "CPDN batch ID" )->capture_default_str();
    cpdn->add_option( "--cpdn_wu", result.args.cpdn_wu, "CPDN workunit ID" )->capture_default_str();
    cpdn->add_option( "--cpdn_app", result.args.cpdn_app, "CPDN application name" )->capture_default_str();
    cpdn->add_option( "--cpdn_upload_int", result.args.cpdn_upload_int, "Upload interval in seconds" )
        ->check( CLI::NonNegativeNumber )
        ->capture_default_str();
    cpdn->add_option( "--cpdn_ancil_files", result.args.cpdn_ancil_files, "Comma-delimited list of ancillary files" )
        ->delimiter( ',' )
        ->capture_default_str();

    app.add_option( "--model_args", result.args.model_args, "Model-specific args passed through as strings" )
        ->delimiter( ',' )
        ->expected( -1 )
        ->capture_default_str();

    try {
        app.parse( argc, argv );
    } catch ( const CLI::ParseError& e ) {
        result.exit_code = app.exit( e );
        result.ok = false;
    }

    return result;
}

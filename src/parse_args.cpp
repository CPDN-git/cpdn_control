#include "parse_args.h"

#include "../tools/CLI11/CLI/CLI.hpp"

ParseResult parse_args( int argc, char** argv )
{
    ParseResult result;

    CLI::App app{ "CPDN control" };
    app.allow_extras();    // allow unknown options (e.g., --nthreads) coming from boinc users.

    auto* cpdn = app.add_option_group( "cpdn" );

    // Options specifically for the control code
    cpdn->add_option( "--batch", result.batch, "CPDN batch ID" )->capture_default_str();
    cpdn->add_option( "--workunit", result.workunit, "CPDN workunit ID" )->capture_default_str();
    cpdn->add_option( "--memberid", result.memberid, "CPDN unique member ID" )->capture_default_str();
    cpdn->add_option( "--app_name", result.app_name, "CPDN application name" )->capture_default_str();
    cpdn->add_option( "--startdate", result.startdate, "Simulation start date" )->capture_default_str();
    cpdn->add_option( "--fcast_len", result.fcast_len, "Forecast length (days)." )->capture_default_str();
    cpdn->add_option( "--upload_interval", result.upload_interval, "Upload interval in seconds" )
        ->check( CLI::NonNegativeNumber )
        ->capture_default_str();
    cpdn->add_option( "--cpdn_ancil_files", result.ancil_files, "Comma-delimited list of ancillary files" )->delimiter( ',' )->capture_default_str();

    // Options to be passed through to the model
    app.add_option( "--model_args", result.model_args, "Model-specific args passed through as strings" )
        ->delimiter( ',' )
        ->expected( -1 )
        ->capture_default_str();

    try {
        app.parse( argc, argv );
    } catch ( const CLI::ParseError& e ) {
        result.exit_code = app.exit( e );
        result.ok = false;
    }
    std::cerr << "DEBUG: startdate = " << result.startdate << "\n";
    std::cerr << "DEBUG: fcast_len = " << result.fcast_len << "\n";
    std::cerr << "DEBUG: batch = " << result.batch << "\n";
    return result;
}

#include "parse_args.h"

#include "../tools/CLI11/CLI/CLI.hpp"

ParseResult parse_args( int argc, char** argv )
{
    ParseResult result;

    CLI::App app{ "CPDN control" };
    app.allow_extras();    // allow unknown options (e.g., --nthreads) coming from boinc users.

    auto* cpdn = app.add_option_group( "cpdn" );

    // Filename-resolution options are CPDN task metadata used to locate the downloaded files.
    // They are not passed into the model and are not authoritative model runtime configuration.
    cpdn->add_option( "--batch", result.batch, "CPDN batch ID" )->capture_default_str();
    cpdn->add_option( "--workunit", result.workunit, "CPDN workunit ID" )->capture_default_str();
    cpdn->add_option( "--memberid", result.memberid, "CPDN unique member ID" )->capture_default_str();
    cpdn->add_option( "--app_name", result.app_name, "CPDN application name" )->capture_default_str();
    cpdn->add_option( "--filename_startdate", result.filename_startdate, "Start-date token embedded in CPDN download filenames." )
        ->capture_default_str();
    cpdn->add_option( "--filename_fclen", result.filename_fclen, "Forecast-length token embedded in CPDN download filenames." )
        ->capture_default_str();
    cpdn->add_option( "--upload_interval", result.upload_interval, "Upload interval in seconds" )
        ->check( CLI::NonNegativeNumber )
        ->capture_default_str();
    cpdn->add_option( "--cpdn_ancil_files", result.ancil_files, "Comma-delimited list of ancillary files" )->delimiter( ',' )->capture_default_str();

    // Options to be passed through to the model (? maybe not needed)
    app.add_option( "--model_args", result.model_args, "Model-specific args passed through as strings" )
        ->delimiter( ',' )
        ->expected( -1 )
        ->capture_default_str();

    // Output command line to stderr for remote debugging
    std::cerr << "Command line arguments:\n";
    for ( int i = 0; i < argc; ++i ) {
        std::cerr << "  argv[" << i << "] = " << argv[i] << '\n';
    }

    // Attempt to parse the command line arguments
    try {
        app.parse( argc, argv );
    } catch ( const CLI::ParseError& e ) {
        result.exit_code = app.exit( e );
        result.ok = false;
    }

    return result;
}

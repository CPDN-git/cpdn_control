#include "parse_args.h"

#include <iostream>
#include <sstream>

#include "cpdn_control.h"

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


bool process_args( const ParseResult& parse_result, TaskConfig& tconfig, std::string& err_msg )
{
    // These CLI values are CPDN task/download naming metadata only.
    // They are used to resolve the app-bundle filename before the model control input is parsed.
    // They are not passed to the model and do not define model runtime behaviour.
    tconfig.filename_startdate = parse_result.filename_startdate;
    tconfig.exptid.clear();                      // Model experiment id is read later from CNMEXP in fort.4.
    tconfig.memberid = parse_result.memberid;    // CPDN's unique member id (umid)
    tconfig.batch = parse_result.batch;          // batch id
    tconfig.workunit = parse_result.workunit;    // workunit id
    tconfig.filename_fclen = parse_result.filename_fclen;

    std::cerr << "Parsed arguments:\n"
              << "  filename_startdate: " << tconfig.filename_startdate << '\n'
              << "  memberid: " << tconfig.memberid << '\n'
              << "  batch: " << tconfig.batch << '\n'
              << "  workunit: " << tconfig.workunit << '\n'
              << "  filename_fclen: " << tconfig.filename_fclen << '\n';

    std::vector<std::string> missing_args;
    if ( tconfig.filename_startdate.empty() ) {
        missing_args.push_back( "--filename_startdate" );
    }
    if ( tconfig.memberid.empty() ) {
        missing_args.push_back( "--memberid" );
    }
    if ( tconfig.batch.empty() ) {
        missing_args.push_back( "--batch" );
    }
    if ( tconfig.workunit.empty() ) {
        missing_args.push_back( "--workunit" );
    }
    if ( tconfig.filename_fclen.empty() ) {
        missing_args.push_back( "--filename_fclen" );
    }

    if ( !missing_args.empty() ) {
        std::ostringstream oss;
        oss << "Missing required controller argument";
        if ( missing_args.size() > 1 ) {
            oss << "s";
        }
        oss << ": ";
        for ( auto it = missing_args.begin(); it != missing_args.end(); ++it ) {
            if ( it != missing_args.begin() ) {
                oss << ", ";
            }
            oss << *it;
        }
        err_msg = oss.str();
        return false;
    }

    err_msg.clear();
    return true;
}

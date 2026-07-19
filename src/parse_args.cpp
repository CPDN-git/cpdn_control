#include "parse_args.h"

#include <cctype>
#include <iostream>
#include <sstream>

#include "cpdn_control.h"

#include "../third_party/CLI11/CLI/CLI.hpp"

namespace {

bool is_valid_filename_label( const std::string& label, std::string& err_msg )
{
    constexpr std::size_t max_filename_label_length = 128;

    if ( label.empty() ) {
        err_msg = "must not be empty";
        return false;
    }
    if ( label.length() > max_filename_label_length ) {
        err_msg = "must be no longer than " + std::to_string( max_filename_label_length ) + " characters";
        return false;
    }
    if ( label == "." || label.find( ".." ) != std::string::npos ) {
        err_msg = "must not contain traversal components";
        return false;
    }
    for ( const unsigned char ch : label ) {
        if ( !std::isalnum( ch ) && ch != '_' && ch != '-' && ch != '.' ) {
            err_msg = "may contain only letters, digits, '_', '-', and '.'";
            return false;
        }
    }
    return true;
}

}    // namespace

ParseResult parse_args( int argc, char** argv )
{
    ParseResult result;

    CLI::App app{ "CPDN control" };
    app.allow_extras();    // allow unknown options (e.g., --nthreads coming a user's app_config.xml file).

    auto* cpdn = app.add_option_group( "cpdn" );

    // filename_label is opaque task metadata used to locate the downloaded app bundle.
    // It is not passed into the model and is deliberately not interpreted as a date or forecast length.
    cpdn->add_option( "--batch", result.batch, "CPDN batch ID" )->capture_default_str();
    cpdn->add_option( "--workunit", result.workunit, "CPDN workunit ID" )->capture_default_str();
    cpdn->add_option( "--memberid", result.memberid, "CPDN unique member ID" )->capture_default_str();
    cpdn->add_option( "--filename_label", result.filename_label, "Opaque filename component embedded in the app-bundle logical filename." )
        ->capture_default_str();
    // upload_interval is controller/task policy in model steps; 0 disables result uploads but does not disable trickles.
    cpdn->add_option( "--upload_interval", result.upload_interval, "Upload interval in model steps" )
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
    // These CLI values are controller/task configuration.
    // filename_label resolves the app bundle before model control input is available.
    // It remains opaque filename metadata; model runtime duration comes from model control input.
    // upload_interval is controller upload policy, not model configuration.
    tconfig.filename_label = parse_result.filename_label;
    tconfig.memberid = parse_result.memberid;    // CPDN's unique member id (umid)
    tconfig.batch = parse_result.batch;          // batch id
    tconfig.workunit = parse_result.workunit;    // workunit id
    tconfig.upload_interval = parse_result.upload_interval;

    std::cerr << "Parsed arguments:\n"
              << "  filename_label: " << tconfig.filename_label << '\n'
              << "  memberid: " << tconfig.memberid << '\n'
              << "  batch: " << tconfig.batch << '\n'
              << "  workunit: " << tconfig.workunit << '\n'
              << "  upload_interval: " << tconfig.upload_interval << '\n';

    std::vector<std::string> missing_args;
    if ( tconfig.filename_label.empty() ) {
        missing_args.push_back( "--filename_label" );
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

    if ( !is_valid_filename_label( tconfig.filename_label, err_msg ) ) {
        err_msg = "Invalid --filename_label: " + err_msg;
        return false;
    }

    err_msg.clear();
    return true;
}

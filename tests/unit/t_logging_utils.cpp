// Test timestamped logging stream wrapper
//
//  Glenn Carver, CPDN, 2026

#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "../../lib/logging_utils.h"
#include "unit_tests.h"

namespace {

std::vector<std::string> split_lines( const std::string& text )
{
    std::vector<std::string> lines;
    std::istringstream in( text );
    std::string line;
    while ( std::getline( in, line ) ) {
        lines.push_back( line );
    }
    return lines;
}


bool has_timestamp_prefix( const std::string& line )
{
    static const std::regex pattern( R"(^\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\] )" );
    return std::regex_search( line, pattern );
}

}    // namespace


int t_logging_utils()
{
    TEST( "t_logging_utils" );

    std::cout << "Subtest: timestamp prefix added at start of each new line\n";
    {
        std::ostringstream out;
        {
            TimestampedCerrGuard guard( out );
            out << "alpha\nbeta\n";
        }

        const auto lines = split_lines( out.str() );
        if ( lines.size() != 2 || !has_timestamp_prefix( lines[0] ) || !has_timestamp_prefix( lines[1] ) ||
             lines[0].find( "alpha" ) == std::string::npos || lines[1].find( "beta" ) == std::string::npos ) {
            FAIL;
            std::cout << "Unexpected prefixed multi-line output:\n" << out.str() << "\n";
            return EXIT_FAILURE;
        }
    }

    std::cout << "Subtest: partial writes share one prefix and buffer is restored on scope exit\n";
    {
        std::ostringstream out;
        {
            TimestampedCerrGuard guard( out );
            out << "gamma";
            out << " delta\n";
        }
        out << "plain\n";

        const auto lines = split_lines( out.str() );
        if ( lines.size() != 2 || !has_timestamp_prefix( lines[0] ) || lines[0].find( "gamma delta" ) == std::string::npos ||
             has_timestamp_prefix( lines[1] ) || lines[1] != "plain" ) {
            FAIL;
            std::cout << "Unexpected partial-write or restored-buffer output:\n" << out.str() << "\n";
            return EXIT_FAILURE;
        }
    }

    SUCCESS;
    return EXIT_SUCCESS;
}

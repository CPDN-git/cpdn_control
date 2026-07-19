// Unit test for controller filename-label argument processing.

#include <cstdlib>
#include <iostream>
#include <string>

#include "../../src/cpdn_control.h"
#include "../../src/parse_args.h"
#include "unit_tests.h"

namespace {

bool process_label( const std::string& label, std::string& err_msg )
{
    ParseResult parsed;
    parsed.batch = "b123";
    parsed.workunit = "456";
    parsed.memberid = "member";
    parsed.filename_label = label;

    TaskConfig config;
    return process_args( parsed, config, err_msg );
}

}    // namespace

int t_filename_label_args()
{
    TEST( "t_filename_label_args" );

    int test_count = 0;
    int test_passed = 0;
    auto expect = [&]( const std::string& label, bool expected ) {
        ++test_count;
        std::string err_msg;
        if ( process_label( label, err_msg ) == expected ) {
            ++test_passed;
        } else {
            std::cerr << "  Test " << test_count << " FAILED for label '" << label << "': " << err_msg << '\n';
        }
    };

    expect( "20220701_london_caseD", true );
    expect( "20220701.1_case-D", true );
    expect( "", false );
    expect( "../case", false );
    expect( "london/caseD", false );
    expect( "london caseD", false );

    std::cout << "  filename_label_args: " << test_passed << "/" << test_count << " tests passed\n";
    if ( test_passed == test_count ) {
        TEST_SUCCESS;
        return EXIT_SUCCESS;
    }

    TEST_FAIL;
    return EXIT_FAILURE;
}

// Test to check reading rcf file
//
//  Glenn Carver, CPDN, 2025

#include <fstream>

#include "../models/openifs/oifs_utils.h"
#include "unit_tests.h"

/**
 * @brief  Test: read_rcf_file.
 */

int t_read_rcf_file()
{
    TEST( "t_read_rcf_file" );

    // Generate test rcf file input (taken from real batch)
    std::ofstream rcf_test( "rcf", std::ios::out | std::ios::trunc );
    rcf_test << "&NAMRCF\n"
             << "CSTEP=\"    1008\",\n"
             << "CTIME=\"00140000      \",\n"
             << "NSTEPLPP=201        ,202        ,49         ,228226     ,228227      5*0          ,\n"
             << "IPRGPNSRES=1          ,\n"
             << "IPRGPEWRES=1          ,\n"
             << "IPRTRWRES=1          ,\n"
             << "IPRTRVRES=1          ,\n"
             << "GMASS0=  98335.474040344547     ,\n"
             << "GMASSI=  98335.393717301660     ,\n"
             << "/\n";
    rcf_test.close();

    std::string ctime;
    std::string cstep;
    std::ifstream rcf_file( "rcf" );

    bool ret = oifs_read_rcf_file( rcf_file, ctime, cstep );
    std::cout << "read_rcf_file : ctime = '" << ctime << "'\n";
    std::cout << "read_rcf_file : cstep = '" << cstep << "'\n";

    if ( ret ) {
        if ( cstep != "1008" || ctime != "00140000" ) {
            FAIL;
            return EXIT_FAILURE;
        }
        SUCCESS;
        return EXIT_SUCCESS;
    }
    FAIL;
    return EXIT_FAILURE;
}
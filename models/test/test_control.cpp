//
// Implementation of the test model control class.
//  Glenn Carver, CPDN, 2025.

#include "test_control.h"
#include "../../lib/utils.h"

// Note: Implementations of the pure virtual functions from ModelControl
// would go here when they are implemented.

/**
 * @brief Print the last n lines of key log files produced by the model.
 * @param nlines Number of lines to print from end of each log file.
 */
void TestControl::print_logs( const int nlines ) const
{
    // TODO: could this be pushed down to the base class rather than re-implemented in each derived class?
    for ( const auto& log_file : log_files ) {
        print_last_lines( log_file, nlines );    // from lib/utils.h; will check file exists
    }
}

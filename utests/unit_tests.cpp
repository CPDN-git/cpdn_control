//
// Unit tests master
//
//   Glenn Carver, CPDN, 2025

#include <map>
#include <functional>

#include "unit_tests.h"



 /****************************************
  * @brief Run tests; pass test name on command line
  * 
  */
 int main(int argc, char* argv[])
 {

    // Map the test name (as set in CMakeLists.txt) to the test function.
    std::map< std::string, std::function<int()> > test_map = {
                {"Read RCF File",       t_read_rcf_file},
                {"Read Progress File",  t_read_progress_file},
                {"CPU Time Comparison", t_cputime_comparison},
                // Add new test functions here! Remember previous trailing comma!
    };

    if (argc != 2) {
        std::cerr << "ERROR: Usage:  <TestName>" << std::endl;
        std::cerr << "See code or CMakeLists.txt for test names." << std::endl;
        return EXIT_FAILURE;                    // Returning failure here is useful if CTest misconfigures the call
    }

    std::string test_name = argv[1];
    auto t_fn = test_map.find(test_name);

    if (t_fn == test_map.end()) {
        std::cerr << "ERROR: Test '" << test_name << "' not found." << std::endl;
        return EXIT_FAILURE;
    }

    // Run the required test
    std::cout << " ---- Running Unit Test : " << test_name << " ----\n" << std::endl;
    auto ret = t_fn->second();
    
    return ret;     // must be either EXIT_FAILURE or EXIT_SUCCESS.
}
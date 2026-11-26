#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "boinc/error_numbers.h"
#include "src/cpdn_control.h"
#include "unit_tests.h"

namespace fs = std::filesystem;

namespace {

void enter_test_slot()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto slot = fs::current_path() / ( "tmp_init_boinc_" + std::to_string( stamp ) );
    fs::create_directory( slot );
    fs::current_path( slot );
    // Each CTest invocation is a separate process. Keep this slot until process
    // exit because BOINC retains its lock file, diagnostic stream and timer.
}

#ifdef __linux__
int thread_count()
{
    const auto tasks = fs::directory_iterator( "/proc/self/task" );
    return static_cast<int>( std::distance( fs::begin( tasks ), fs::end( tasks ) ) );
}
#endif

int check_invalid_init_data( const bool malformed )
{
    enter_test_slot();
    if ( malformed ) {
        std::ofstream( "init_data.xml" ) << "<not_app_init_data/>\n";
    }

    BoincConfig config{};
    const int result = init_boinc( config );
    const int expected = malformed ? ERR_XML_PARSE : ERR_FOPEN;
    if ( result != expected ) {
        std::cout << "Expected initialization error " << expected << ", got " << result << '\n';
        return EXIT_FAILURE;
    }

    std::ifstream log( "stderr.txt" );
    const std::string diagnostics( ( std::istreambuf_iterator<char>( log ) ), std::istreambuf_iterator<char>() );
    const std::string expected_error = "boinc_parse_init_data_file failed (" + std::to_string( expected ) + "): " + boincerror( expected );
    if ( diagnostics.find( expected_error ) == std::string::npos ) {
        std::cout << "Missing BOINC parse error context in stderr.txt\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

}    // namespace

int t_init_boinc_standalone()
{
    enter_test_slot();
    std::ofstream( "init_data.xml" ) << "<app_init_data>\n"
                                        "<app_version>206</app_version>\n"
                                        "<app_name>test_model</app_name>\n"
                                        "<project_dir>test_project</project_dir>\n"
                                        "<boinc_dir>test_boinc</boinc_dir>\n"
                                        "<wu_name>test_workunit</wu_name>\n"
                                        "<result_name>test_result</result_name>\n"
                                        "<ncpus>4</ncpus>\n"
                                        "<shm_key>0</shm_key>\n"
                                        "</app_init_data>\n";
#ifdef __linux__
    const int threads_before = thread_count();
#endif
    BoincConfig config{};
    const int result = init_boinc( config );
    if ( result != 0 || !config.standalone || config.app_version != "2.06" || config.app_name != "test_model" ||
         config.project_dir != "test_project/" || config.boinc_dir != "test_boinc/" || config.wu_name != "test_workunit" ||
         config.result_name != "test_result" || config.ncpus != 4 || config.slot_path != fs::current_path().string() ) {
        std::cout << "Standalone BOINC initialization failed or returned incorrect task metadata; code=" << result << '\n';
        return EXIT_FAILURE;
    }
#ifdef __linux__
    const int threads_after = thread_count();
    if ( threads_after != threads_before + 1 ) {
        std::cout << "Expected one BOINC timer thread; threads before=" << threads_before << ", after=" << threads_after << '\n';
        return EXIT_FAILURE;
    }
#endif
    return EXIT_SUCCESS;
}

int t_init_boinc_missing_data() { return check_invalid_init_data( false ); }

int t_init_boinc_malformed_data() { return check_invalid_init_data( true ); }

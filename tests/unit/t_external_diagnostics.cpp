#include "lib/utils.h"
#include "models/openifs/external_diagnostics.h"
#include "unit_tests.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

int t_external_diagnostics()
{
    namespace fs = std::filesystem;
    TEST( "t_external_diagnostics" );
    const auto dir = fs::temp_directory_path() / "cpdn_external_diagnostics_test";
    fs::create_directories( dir );
    const auto output = dir / "trickle_data";
    int failures = 0;
    auto expect = [&]( bool ok, const char* message ) {
        if ( !ok ) {
            std::cerr << message << '\n';
            ++failures;
        }
    };
    auto check = [&]( const TimedProcessResult& result, bool accepted, const std::string& expected_log ) {
        std::ofstream( output ) << "diagnostic payload\n";
        std::ostringstream log;
        auto* saved_log = std::cerr.rdbuf( log.rdbuf() );
        const bool actual = accept_diagnostics_result( result, output );
        std::cerr.rdbuf( saved_log );
        expect( actual == accepted, "Unexpected diagnostics acceptance" );
        expect( fs::exists( output ) == accepted, "Rejected diagnostics payload was retained or accepted data removed" );
        expect( log.str().find( expected_log ) != std::string::npos, "Missing diagnostics outcome log" );
        expect( ( log.str().find( "Diagnostics completed and updated trickle_data" ) != std::string::npos ) == accepted,
                "Diagnostics success log disagrees with acceptance" );
        return log.str();
    };

    TimedProcessResult result;
    result.status = TimedProcessStatus::success;
    result.exit_code = 0;
    result.output_updated = true;
    result.cleanup.status = ChildCleanupStatus::terminated;
    check( result, true, "Diagnostics completed" );
    result.cleanup.status = ChildCleanupStatus::already_exited;
    check( result, true, "Diagnostics completed" );

    // A successful leader and fresh file must not hide failed or unconfirmed cleanup.
    result.cleanup.status = ChildCleanupStatus::failed;
    result.cleanup.message = "termination request denied";
    expect( check( result, false, "termination_failed" ).find( result.cleanup.message ) != std::string::npos, "Cleanup failure detail was lost" );
    result.cleanup.status = ChildCleanupStatus::timed_out;
    result.cleanup.message = "exit unconfirmed";
    result.cleanup.elapsed = std::chrono::milliseconds( 2000 );
    expect( check( result, false, "deadline_expired" ).find( "elapsed_ms=2000" ) != std::string::npos, "Cleanup deadline context was lost" );

    result.cleanup = {};
    result.cleanup.status = ChildCleanupStatus::terminated;
    result.status = TimedProcessStatus::child_failed;
    result.exit_code = 3;
    check( result, false, "exit_code=3" );
    result.status = TimedProcessStatus::success;
    result.exit_code = 0;
    result.output_updated = false;
    check( result, false, "did not produce a fresh trickle_data" );

    fs::remove_all( dir );
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

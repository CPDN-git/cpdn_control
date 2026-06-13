// Test to check parsing the model control input via the model layer.
//
//  Glenn Carver, CPDN, 2026

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "../models/openifs/oifs_control.h"
#include "../models/wrf/wrf_control.h"
#include "unit_tests.h"

namespace fs = std::filesystem;

static bool write_control_input( const fs::path& path, const std::string& content )
{
    std::ofstream out( path, std::ios::out | std::ios::trunc );
    if ( !out.is_open() ) {
        return false;
    }
    out << content;
    return static_cast<bool>( out );
}

int t_parse_control_input()
{
    TEST( "t_parse_control_input" );

    const fs::path original_cwd = fs::current_path();
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path tmp_dir = fs::temp_directory_path() / ( "cpdn_parse_control_input_" + std::to_string( now ) );
    std::error_code ec;
    fs::create_directories( tmp_dir, ec );
    if ( ec ) {
        TEST_FAIL;
        std::cout << "Unable to create temp dir: " << tmp_dir.string() << "\n";
        return EXIT_FAILURE;
    }

    OpenIFSControl model( "ECMWF", "oifs_43r3_omp_l159", "1.0.0", "oifs_43r3_omp_model.exe" );
    fs::current_path( tmp_dir );

    const std::string valid_content = "&NAMFPC\n"
                                      " CFPFMT=\"MODEL\",\n"
                                      "/\n\n"
                                      "&NAMARG\n"
                                      " UTSTEP=3600.0,\n"
                                      " CUSTOP=48,\n"
                                      " CNMEXP='ABCD',\n"
                                      "/\n"
                                      "&NAMRES\n"
                                      " NFRRES=-24,\n"
                                      "/\n"
                                      "&NAMCT0\n"
                                      " NFRPOS=6,\n"
                                      "/\n";

    if ( !write_control_input( tmp_dir / "fort.4", valid_content ) ) {
        TEST_FAIL;
        std::cout << "Unable to write valid fort.4 test file\n";
        fs::current_path( original_cwd );
        fs::remove_all( tmp_dir, ec );
        return EXIT_FAILURE;
    }

    std::cout << "Subtest: parse valid control input\n";
    auto parsed = model.parse_control_input();
    if ( !parsed.ok || parsed.timestep_seconds != 3600 || parsed.output_interval != 6 || parsed.restart_interval != 24 || parsed.total_steps != 48 ||
         parsed.forecast_length_time != 172800.0 ) {
        TEST_FAIL;
        std::cout << "Unexpected parse result:" << " ok=" << parsed.ok << ", timestep_seconds=" << parsed.timestep_seconds
                  << ", output_interval=" << parsed.output_interval << ", restart_interval=" << parsed.restart_interval
                  << ", total_steps=" << parsed.total_steps << ", forecast_length_time=" << parsed.forecast_length_time << "\n";
        fs::current_path( original_cwd );
        fs::remove_all( tmp_dir, ec );
        return EXIT_FAILURE;
    }

    std::cout << "Subtest: validate missing required field handling\n";
    const std::string invalid_content = "&NAMARG\n"
                                        " UTSTEP=3600.0,\n"
                                        "/\n"
                                        "&NAMRES\n"
                                        " NFRRES=24,\n"
                                        "/\n"
                                        "&NAMCT0\n"
                                        " NFRPOS=6,\n"
                                        "/\n";
    if ( !write_control_input( tmp_dir / "fort.4", invalid_content ) ) {
        TEST_FAIL;
        std::cout << "Unable to write invalid fort.4 test file\n";
        fs::current_path( original_cwd );
        fs::remove_all( tmp_dir, ec );
        return EXIT_FAILURE;
    }

    parsed = model.parse_control_input();
    if ( parsed.ok || parsed.error_step != "validate" ) {
        TEST_FAIL;
        std::cout << "Expected validate failure, got ok=" << parsed.ok << ", error_step=" << parsed.error_step
                  << ", error_message=" << parsed.error_message << "\n";
        fs::current_path( original_cwd );
        fs::remove_all( tmp_dir, ec );
        return EXIT_FAILURE;
    }

    std::cout << "Subtest: parse WRF control input output interval\n";
    WRFControl wrf_model( "UCAR", "wrf_4.6.1_urban", "4.6.1", "wrf_4.6.1_urban.exe" );
    const std::string wrf_content = "&time_control\n"
                                    " run_days = 0,\n"
                                    " run_hours = 1,\n"
                                    " run_minutes = 0,\n"
                                    " run_seconds = 0,\n"
                                    " history_interval = 9999, 9999, 60,\n"
                                    " frames_per_outfile = 1, 1, 24,\n"
                                    " restart_interval = 180,\n"
                                    "/\n"
                                    "&domains\n"
                                    " time_step = 300,\n"
                                    " max_dom = 3,\n"
                                    "/\n";
    if ( !write_control_input( tmp_dir / "namelist.input", wrf_content ) ) {
        TEST_FAIL;
        std::cout << "Unable to write WRF namelist.input test file\n";
        fs::current_path( original_cwd );
        fs::remove_all( tmp_dir, ec );
        return EXIT_FAILURE;
    }

    parsed = wrf_model.parse_control_input();
    if ( !parsed.ok || parsed.timestep_seconds != 300 || parsed.output_interval != 288 || parsed.restart_interval != 36 || parsed.total_steps != 12 ||
         parsed.forecast_length_time != 3600.0 ) {
        TEST_FAIL;
        std::cout << "Unexpected WRF parse result:" << " ok=" << parsed.ok << ", timestep_seconds=" << parsed.timestep_seconds
                  << ", output_interval=" << parsed.output_interval << ", restart_interval=" << parsed.restart_interval
                  << ", total_steps=" << parsed.total_steps << ", forecast_length_time=" << parsed.forecast_length_time << "\n";
        fs::current_path( original_cwd );
        fs::remove_all( tmp_dir, ec );
        return EXIT_FAILURE;
    }

    fs::current_path( original_cwd );
    fs::remove_all( tmp_dir, ec );
    TEST_SUCCESS;
    return EXIT_SUCCESS;
}

// Test to check parsing the model control input via the model layer.
//
//  Glenn Carver, CPDN, 2026

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "../models/openifs/oifs_control.h"
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
        FAIL;
        std::cout << "Unable to create temp dir: " << tmp_dir.string() << "\n";
        return EXIT_FAILURE;
    }

    OpenIFSControl model( "ECMWF", "oifs_43r3_omp_l159", "1.0.0", "oifs_43r3_omp_model.exe" );
    fs::current_path( tmp_dir );

    const std::string valid_content = "&NAMFPC\n"
                                      " CFPFMT=\"MODEL\",\n"
                                      "/\n\n"
                                      "!HORIZ_RESOLUTION=159\n"
                                      "!VERT_RESOLUTION=91\n"
                                      "!GRID_TYPE=l_2\n"
                                      "!UPLOAD_INTERVAL=6\n"
                                      "\n"
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
        FAIL;
        std::cout << "Unable to write valid fort.4 test file\n";
        fs::current_path( original_cwd );
        fs::remove_all( tmp_dir, ec );
        return EXIT_FAILURE;
    }

    std::cout << "Subtest: parse valid control input\n";
    auto parsed = model.parse_control_input();
    if ( !parsed.ok || parsed.horiz_resolution != "159" || parsed.vert_resolution != "91" || parsed.grid_type != "l_2" ||
         parsed.experiment_id != "ABCD" || parsed.upload_interval != 6 || parsed.timestep_seconds != 3600 || parsed.output_interval != 6 ||
         parsed.restart_interval != -24 || parsed.total_steps != 48 || parsed.forecast_length_time != 172800.0 ) {
        FAIL;
        std::cout << "Unexpected parse result:" << " ok=" << parsed.ok << ", horiz=" << parsed.horiz_resolution << ", vert=" << parsed.vert_resolution
                  << ", grid=" << parsed.grid_type << ", exptid=" << parsed.experiment_id << ", upload=" << parsed.upload_interval
                  << ", timestep_seconds=" << parsed.timestep_seconds << ", output_interval=" << parsed.output_interval
                  << ", restart_interval=" << parsed.restart_interval << ", total_steps=" << parsed.total_steps
                  << ", forecast_length_time=" << parsed.forecast_length_time << "\n";
        fs::current_path( original_cwd );
        fs::remove_all( tmp_dir, ec );
        return EXIT_FAILURE;
    }

    std::cout << "Subtest: validate missing required field handling\n";
    const std::string invalid_content = "&NAMARG\n"
                                        " UTSTEP=3600.0,\n"
                                        " CUSTOP=48,\n"
                                        "/\n"
                                        "&NAMRES\n"
                                        " NFRRES=24,\n"
                                        "/\n"
                                        "&NAMCT0\n"
                                        " NFRPOS=6,\n"
                                        "/\n";
    if ( !write_control_input( tmp_dir / "fort.4", invalid_content ) ) {
        FAIL;
        std::cout << "Unable to write invalid fort.4 test file\n";
        fs::current_path( original_cwd );
        fs::remove_all( tmp_dir, ec );
        return EXIT_FAILURE;
    }

    parsed = model.parse_control_input();
    if ( parsed.ok || parsed.error_step != "validate" ) {
        FAIL;
        std::cout << "Expected validate failure, got ok=" << parsed.ok << ", error_step=" << parsed.error_step
                  << ", error_message=" << parsed.error_message << "\n";
        fs::current_path( original_cwd );
        fs::remove_all( tmp_dir, ec );
        return EXIT_FAILURE;
    }

    fs::current_path( original_cwd );
    fs::remove_all( tmp_dir, ec );
    SUCCESS;
    return EXIT_SUCCESS;
}

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "../models/openifs/oifs_control.h"
#include "../src/cpdn_control.h"
#include "../src/upload_manager.h"
#include "../zip/cpdn_zip.h"
#include "unit_tests.h"

namespace fs = std::filesystem;

int t_upload_manager()
{
    TEST( "t_upload_manager" );

    const fs::path test_root = "upload_manager_test";
    const fs::path slot_dir = test_root / "slot";
    const fs::path project_dir = test_root / "project";
    const fs::path upload_dir = project_dir / "uploads";
    const fs::path extract_dir = test_root / "extract";

    if ( fs::exists( test_root ) ) {
        fs::remove_all( test_root );
    }

    fs::create_directories( slot_dir );
    fs::create_directories( upload_dir );
    fs::create_directories( extract_dir );

    std::ofstream( slot_dir / "ifs.stat" ) << "dummy log\n";
    std::ofstream( upload_dir / "ICMSHTEST+000000" ) << "dummy output\n";

    BoincConfig bconfig;
    bconfig.project_dir = project_dir.string() + "/";
    bconfig.slot_path = slot_dir.string();
    bconfig.standalone = true;

    OpenIFSControl model_ctrl( "CPDN", "test_model", "1.0", "test_model" );
    UploadManager upload_manager( bconfig, model_ctrl, upload_dir, "result_base", 36, 12 );

    TaskState tstate;
    tstate.upload_file_number = 1;
    tstate.last_upload_step = 12;
    tstate.last_completed_step = 18;

    BoincRuntime runtime;
    auto finalize_result = upload_manager.finalize_remaining_uploads( runtime, tstate, tstate.last_completed_step, true, false );

    bool ok = finalize_result.ok;
    ok = ok && fs::exists( project_dir / "result_base_1.zip" );
    ok = ok && fs::exists( project_dir / "result_base_2.zip" );
    ok = ok && tstate.upload_file_number == 3;

    bool unzip_ok = cpdn_unzip( project_dir / "result_base_1.zip", extract_dir / "upload1" );
    unzip_ok = unzip_ok && cpdn_unzip( project_dir / "result_base_2.zip", extract_dir / "upload2" );

    const bool upload1_has_output = fs::exists( extract_dir / "upload1" / "ICMSHTEST+000000" );
    const bool upload1_has_log = fs::exists( extract_dir / "upload1" / "ifs.stat" );
    const bool upload2_has_placeholder = fs::exists( extract_dir / "upload2" / "cpdn_upload_placeholder_2.txt" );

    const bool cleanup_ok = upload_manager.cleanup_upload_dir();
    const bool upload_dir_removed = !fs::exists( upload_dir );
    if ( fs::exists( test_root ) ) {
        fs::remove_all( test_root );
    }

    if ( ok && unzip_ok && upload1_has_output && upload1_has_log && upload2_has_placeholder && cleanup_ok && upload_dir_removed ) {
        TEST_SUCCESS;
        return EXIT_SUCCESS;
    }

    std::cerr << "  Upload manager standalone finalization test failed\n";
    TEST_FAIL;
    return EXIT_FAILURE;
}

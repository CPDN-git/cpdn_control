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

    const fs::path test_root = fs::absolute( "upload_manager_test" );
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
    std::ofstream( slot_dir / "fort.4" ) << "&NAMARG\n UTSTEP=3600.0,\n CUSTOP=36,\n CNMEXP='TES1',\n/\n"
                                            "&NAMRES\n NFRRES=12,\n/\n&NAMCT0\n NFRPOS=6,\n/\n";

    BoincConfig bconfig;
    bconfig.project_dir = project_dir.string() + "/";
    bconfig.slot_path = slot_dir.string();
    bconfig.standalone = true;

    OpenIFSControl model_ctrl( "CPDN", "test_model", "1.0", "test_model" );
    const auto original_cwd = fs::current_path();
    fs::current_path( slot_dir );
    const auto parsed = model_ctrl.parse_control_input();
    fs::current_path( original_cwd );
    if ( !parsed.ok ) {
        std::cerr << "  Failed to parse TES1 control input\n";
        fs::remove_all( test_root );
        TEST_FAIL;
        return EXIT_FAILURE;
    }

    UploadManager upload_manager( bconfig, model_ctrl, upload_dir, "result_base", 36, 12 );

    TaskState tstate;
    BoincRuntime runtime;

    // Exercise slot staging and a normal scheduled upload with an alphanumeric ID.
    const auto scheduled_filenames = model_ctrl.get_output_filenames( 0 );
    for ( const auto& filename : scheduled_filenames ) {
        std::ofstream( slot_dir / filename ) << "scheduled model output\n";
    }
    upload_manager.move_copyable_output_files( 12 );
    const auto scheduled_result = upload_manager.process_scheduled_upload( runtime, tstate, 12 );
    bool scheduled_ok = scheduled_result.ok && tstate.upload_file_number == 1 && tstate.last_upload_step == 12;
    scheduled_ok = scheduled_ok && cpdn_unzip( project_dir / "result_base_0.zip", extract_dir / "scheduled" );
    for ( const auto& filename : scheduled_filenames ) {
        scheduled_ok = scheduled_ok && fs::exists( extract_dir / "scheduled" / filename ) && !fs::exists( slot_dir / filename ) &&
                       !fs::exists( upload_dir / filename );
    }
    scheduled_ok = scheduled_ok && !fs::exists( extract_dir / "scheduled" / "cpdn_upload_placeholder_0.txt" );

    // An interrupted run preserves available output and logs, then fills the missing slot.
    std::ofstream( slot_dir / "ICMSHTES1+000012" ) << "remaining model output\n";
    tstate.last_completed_step = 18;
    auto finalize_result = upload_manager.finalize_remaining_uploads( runtime, tstate, tstate.last_completed_step, true, false );

    bool ok = finalize_result.ok;
    ok = ok && fs::exists( project_dir / "result_base_1.zip" );
    ok = ok && fs::exists( project_dir / "result_base_2.zip" );
    ok = ok && tstate.upload_file_number == 3;

    bool unzip_ok = cpdn_unzip( project_dir / "result_base_1.zip", extract_dir / "upload1" );
    unzip_ok = unzip_ok && cpdn_unzip( project_dir / "result_base_2.zip", extract_dir / "upload2" );

    const bool upload1_has_output = fs::exists( extract_dir / "upload1" / "ICMSHTES1+000012" );
    const bool upload1_has_log = fs::exists( extract_dir / "upload1" / "ifs.stat" );
    const bool upload2_has_placeholder = fs::exists( extract_dir / "upload2" / "cpdn_upload_placeholder_2.txt" );

    // Successful completion still needs the final archive, including model logs.
    UploadManager success_manager( bconfig, model_ctrl, upload_dir, "success_base", 24, 12 );
    TaskState success_state;
    success_state.upload_file_number = 1;
    success_state.last_upload_step = 12;
    success_state.last_completed_step = 24;
    success_state.model_success = true;
    std::ofstream( slot_dir / "ifs.stat" ) << "successful model log\n";
    std::ofstream( slot_dir / "ICMSHTES1+000024" ) << "final model output\n";
    const auto success_result = success_manager.finalize_remaining_uploads( runtime, success_state, 24, true, false );
    bool success_ok = success_result.ok && success_state.upload_file_number == 2;
    success_ok = success_ok && cpdn_unzip( project_dir / "success_base_1.zip", extract_dir / "success" );
    success_ok = success_ok && fs::exists( extract_dir / "success" / "ICMSHTES1+000024" ) && fs::exists( extract_dir / "success" / "ifs.stat" ) &&
                 !fs::exists( extract_dir / "success" / "cpdn_upload_placeholder_1.txt" ) && !fs::exists( project_dir / "success_base_2.zip" );

    // Keep the diagnostic placeholder for an empty scheduled upload.
    UploadManager empty_manager( bconfig, model_ctrl, upload_dir, "empty_base", 24, 12 );
    TaskState empty_state;
    const auto empty_result = empty_manager.process_scheduled_upload( runtime, empty_state, 12 );
    bool empty_ok = empty_result.ok && empty_state.upload_file_number == 1 && empty_state.last_upload_step == 12;
    empty_ok = empty_ok && cpdn_unzip( project_dir / "empty_base_0.zip", extract_dir / "empty" );
    empty_ok = empty_ok && fs::exists( extract_dir / "empty" / "cpdn_upload_placeholder_0.txt" );

    const bool cleanup_ok = upload_manager.cleanup_upload_dir();
    const bool upload_dir_removed = !fs::exists( upload_dir );
    if ( fs::exists( test_root ) ) {
        fs::remove_all( test_root );
    }

    if ( scheduled_ok && ok && unzip_ok && upload1_has_output && upload1_has_log && upload2_has_placeholder && success_ok && empty_ok && cleanup_ok &&
         upload_dir_removed ) {
        TEST_SUCCESS;
        return EXIT_SUCCESS;
    }

    std::cerr << "  Upload manager standalone test failed: scheduled=" << scheduled_ok
              << ", finalization=" << ( ok && unzip_ok && upload1_has_output && upload1_has_log && upload2_has_placeholder )
              << ", success=" << success_ok << ", empty=" << empty_ok << '\n';
    TEST_FAIL;
    return EXIT_FAILURE;
}

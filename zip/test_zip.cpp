#include "cpdn_zip.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cassert>

int main() {
    // --- Setup Test Environment ---
    const std::filesystem::path test_dir = "zip_tdir";
    const std::filesystem::path slot_dir = test_dir / "slots/0";
    const std::filesystem::path zip_archive_path = slot_dir / "oifs_43r3_omp_l159_app.zip";
    const std::filesystem::path extraction_dir = slot_dir;
    const std::filesystem::path app_path = test_dir / "oifs_43r3_omp_l159.exe";
    const std::string app_content = "This is the content of test using ZipLib.";

    // Clean up previous test runs
    std::filesystem::remove_all(test_dir);

    // Create directories
    std::filesystem::create_directory(test_dir);
    std::filesystem::create_directories(slot_dir);

    // Create a dummy file to be zipped
    {
        std::ofstream app(app_path);
        app << app_content;
    }
    std::cout << "Setup: Created test app file '" << app_path << "'" << std::endl;

    // --- Test cpdn_zip ---
    std::cout << "\n--- Testing cpdn_zip ---" << std::endl;
    std::vector<std::filesystem::path> files_to_zip = { app_path };
    bool zip_result = cpdn_zip(zip_archive_path, files_to_zip);

    std::cout << "cpdn_zip returned: " << (zip_result ? "success" : "failure") << std::endl;
    assert(zip_result && "cpdn_zip should return true on success.");
    assert(std::filesystem::exists(zip_archive_path) && "Zip archive should be created.");
    std::cout << "SUCCESS: cpdn_zip created '" << zip_archive_path << "'" << std::endl;

    // --- Test cpdn_unzip ---
    std::cout << "\n--- Testing cpdn_unzip ---" << std::endl;
    bool unzip_result = cpdn_unzip(zip_archive_path, extraction_dir);

    std::cout << "cpdn_unzip returned: " << (unzip_result ? "success" : "failure") << std::endl;
    assert(unzip_result && "cpdn_unzip should return true on success.");

    // --- Verification ---
    std::cout << "\n--- Verifying Results ---" << std::endl;
    std::filesystem::path extracted_file_path = extraction_dir / app_path.filename();
    assert(std::filesystem::exists(extracted_file_path) && "Extracted file should exist.");
    std::cout << "SUCCESS: Found extracted file '" << extracted_file_path << "'" << std::endl;

    // Verify content
    std::string extracted_content;
    {
        std::ifstream extracted_file(extracted_file_path);
        std::getline(extracted_file, extracted_content);
    }
    assert(extracted_content == app_content && "Extracted file content must match original.");
    std::cout << "SUCCESS: Extracted file content matches original." << std::endl;

    // --- Clean up ---
    //std::cout << "\nCleaning up test directory..." << std::endl;
    //std::filesystem::remove_all(test_dir);

    std::cout << "\nAll tests passed successfully!" << std::endl;

    return 0;
}

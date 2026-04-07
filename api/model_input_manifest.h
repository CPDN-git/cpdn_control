#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct ModelInputManifestContext {
    std::string workunit_id;
    std::string horiz_resolution;
    std::string grid_type;
};

struct ModelInputArchive {
    std::string logical_name;
    std::filesystem::path unzip_relative_dir;
};

using ModelInputManifest = std::vector<ModelInputArchive>;

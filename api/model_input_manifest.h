#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct ModelInputArchive {
    std::string logical_name;
    std::filesystem::path unzip_relative_dir;
};

using ModelInputManifest = std::vector<ModelInputArchive>;

#pragma once

#include <filesystem>

namespace crisp
{
struct AssetPaths
{
    std::filesystem::path shaderSourceDir;
    std::filesystem::path resourceDir;
    std::filesystem::path spvShaderDir;
};
} // namespace crisp

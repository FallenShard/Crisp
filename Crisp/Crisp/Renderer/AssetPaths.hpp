#pragma once

#include <filesystem>

namespace crisp {
struct AssetPaths {
    std::filesystem::path shaderSourceDir;
    std::filesystem::path resourceDir;
    std::filesystem::path spvShaderDir;

    inline std::filesystem::path getSpvShaderPath(const std::string& shaderName) const {
        return spvShaderDir / (shaderName + ".spv");
    }
};
} // namespace crisp

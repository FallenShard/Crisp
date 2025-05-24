#pragma once

#include <filesystem>

namespace crisp {
struct AssetPaths {
    std::filesystem::path shaderSourceDir;
    std::filesystem::path resourceDir;
    std::filesystem::path spvShaderDir;
    std::filesystem::path outputDir;

    std::filesystem::path getShaderSpvPath(const std::string& shaderName) const {
        return spvShaderDir / (shaderName + ".spv");
    }

    std::filesystem::path getShaderSourcePath(const std::string& shaderName) const {
        return shaderSourceDir / (shaderName + ".glsl");
    }

    std::filesystem::path getPipelineConfigPath(const std::string_view pipelineFilename) const {
        return resourceDir / "Pipelines" / pipelineFilename;
    }
};
} // namespace crisp

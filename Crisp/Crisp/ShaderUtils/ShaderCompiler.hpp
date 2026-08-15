#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

#include <Crisp/Core/Result.hpp>

namespace crisp {
struct ShaderCompilationStats {
    uint32_t recompiledShaderCount{0};
    uint32_t skippedShaderCount{0};
};

Result<std::vector<uint32_t>> compileGlslShader(
    const std::filesystem::path& inputPath, std::string_view shaderType = {});
Result<> compileGlslShader(
    const std::filesystem::path& inputPath, const std::filesystem::path& outputPath, std::string_view shaderType = {});

Result<ShaderCompilationStats> recompileShaderDir(
    const std::filesystem::path& inputDir, const std::filesystem::path& outputDir);
} // namespace crisp

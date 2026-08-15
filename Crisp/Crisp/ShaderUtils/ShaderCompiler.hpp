#pragma once

#include <cstdint>
#include <filesystem>

#include <Crisp/Core/Result.hpp>

namespace crisp {
struct ShaderCompilationStats {
    uint32_t recompiledShaderCount{0};
    uint32_t skippedShaderCount{0};
};

Result<ShaderCompilationStats> recompileShaderDir(
    const std::filesystem::path& inputDir, const std::filesystem::path& outputDir);
} // namespace crisp
